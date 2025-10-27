#!/usr/bin/env python3
import argparse, os, math
import torch
from torch.utils.data import DataLoader
from dataset import FenScoreDataset
from model import NoxNet


def train_epoch(model, loader, opt, device, clip=1000.0):
    model.train()
    mse = torch.nn.MSELoss()
    total_loss = 0.0
    n = 0
    for x, y in loader:
        x = x.to(device)
        y = y.to(device)
        # clip targets
        y = torch.clamp(y, -clip, clip)
        opt.zero_grad()
        pred = model(x)
        loss = mse(pred, y)
        loss.backward()
        opt.step()
        total_loss += loss.item() * x.size(0)
        n += x.size(0)
    return total_loss / max(1, n)


def evaluate(model, loader, device, clip=1000.0):
    model.eval()
    mse = torch.nn.MSELoss(reduction='sum')
    tot = 0.0
    n = 0
    with torch.no_grad():
        for x, y in loader:
            x = x.to(device)
            y = torch.clamp(y.to(device), -clip, clip)
            pred = model(x)
            tot += mse(pred, y).item()
            n += x.size(0)
    return tot / max(1, n)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--train', required=True, help='training file (lines: FEN ; score_cp)')
    ap.add_argument('--valid', help='validation file (optional)')
    ap.add_argument('--batch', type=int, default=1024)
    ap.add_argument('--epochs', type=int, default=2)
    ap.add_argument('--lr', type=float, default=1e-3)
    ap.add_argument('--h1', type=int, default=512)
    ap.add_argument('--h2', type=int, default=64)
    ap.add_argument('--out', required=True, help='output .nox path')
    args = ap.parse_args()

    device = 'cuda' if torch.cuda.is_available() else 'cpu'

    train_ds = FenScoreDataset(args.train)
    train_loader = DataLoader(train_ds, batch_size=args.batch, shuffle=True, num_workers=0)

    valid_loader = None
    if args.valid:
        valid_loader = DataLoader(FenScoreDataset(args.valid), batch_size=args.batch, shuffle=False, num_workers=0)

    model = NoxNet(input_dim=782, h1=args.h1, h2=args.h2).to(device)
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr)

    best_val = math.inf
    for ep in range(1, args.epochs+1):
        tr_loss = train_epoch(model, train_loader, opt, device)
        if valid_loader:
            val = evaluate(model, valid_loader, device)
            if val < best_val:
                best_val = val
                model.export_nox(args.out)
        else:
            # no validation: export each epoch
            model.export_nox(args.out)
        print(f"epoch {ep}: train_mse={tr_loss:.3f} val_mse={(val if valid_loader else tr_loss):.3f}")

    # final export
    model.export_nox(args.out)
    print(f"exported {args.out}")

if __name__ == '__main__':
    main()
