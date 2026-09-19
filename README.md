# ♟️ UCI Chess Engine

A high-performance chess engine written in **C++** using **bitboards**, **Minimax with alpha-beta pruning**, and **NNUE-based evaluation**. Built from scratch with modularity and scalability in mind, the engine reaches an estimated ELO of **2600**.

GitHub → [github.com/NoxiousTab/nox_engine](https://github.com/NoxiousTab/nox_engine)

---

##  Features

-  **UCI-compatible** chess engine
-  **Bitboard architecture** for efficient board representation
-  **Minimax with Alpha-Beta Pruning** for optimal move search
-  **NNUE evaluation** integrated for modern position analysis
-  **FEN support** for position loading and game state tracking
-  Legal move generation, check detection, captures, promotions

---

## Engine Architecture

- 12 Bitboards (one per piece type & color) track board state efficiently.
- Move generation includes sliding attacks, castling, en passant, and promotions.
- NNUE evaluates static positions based on neural network weights.

---

## Core Concepts

- **Bitboards**: Each piece type is tracked on a 64-bit int; enables fast shifts & masks.
- **Move Generation**: Pseudo-legal moves generated using precomputed tables.
- **Search**: Minimax with alpha-beta achieves optimal pruning and depth-first search.
- **Evaluation**: Combines static heuristics with neural network inference for strength.
- **FEN Support**: Load arbitrary positions via standard notation.

---

##  Performance

- Processes ~10,000 positions/sec on a modern CPU
- Evaluates with NNUE-backed scoring at ~1600 ELO (estimated by self-play)
- Modular architecture supports adding:
  - Quiescence Search
  - Transposition Tables
  - Multi-threaded search (future scope)

---

## Why This Project Matters

This project reflects deep understanding of:

- **Game theory and adversarial AI**
- **Search space optimization**
- **Low-level performance tuning**
- **Neural network integration in systems**

---

## Future Enhancements

- Quiescence Search & Iterative Deepening
- Multi-threaded search with thread pools
- Transposition tables and Zobrist hashing
- Custom trained NNUE model for evaluations

---

## 👨‍💻 Author

**Tabish Ahmed**  
[GitHub](https://github.com/NoxiousTab) • [LinkedIn](https://linkedin.com/in/ahmed-tabish)

---
