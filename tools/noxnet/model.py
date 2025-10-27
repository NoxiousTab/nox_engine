import torch
import torch.nn as nn

class NoxNet(nn.Module):
    def __init__(self, input_dim=782, h1=512, h2=64):
        super().__init__()
        self.input_dim = input_dim
        self.h1 = h1
        self.h2 = h2
        self.fc1 = nn.Linear(input_dim, h1)
        self.fc2 = nn.Linear(h1, h2)
        self.fc3 = nn.Linear(h2, 1)

    def forward(self, x):
        x = torch.relu(self.fc1(x))
        x = torch.relu(self.fc2(x))
        x = self.fc3(x)
        return x.squeeze(-1)  # [B]

    def export_nox(self, out_path):
        import struct
        MAGIC = b"NOXNET1\x00"
        with open(out_path, 'wb') as f:
            f.write(MAGIC)
            f.write(struct.pack('<I', 1))
            f.write(struct.pack('<I', self.input_dim))
            f.write(struct.pack('<I', self.h1))
            f.write(struct.pack('<I', self.h2))
            f.write(struct.pack('<I', 1))
            for t in [self.fc1.weight, self.fc1.bias,
                      self.fc2.weight, self.fc2.bias,
                      self.fc3.weight, self.fc3.bias]:
                arr = t.detach().cpu().contiguous().view(-1).numpy().astype('float32')
                f.write(arr.tobytes())
