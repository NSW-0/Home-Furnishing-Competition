# Project 1 — Home Furnishing Competition
**Course:** ENCS4330 — Real-Time Applications & Embedded Systems
**University:** Birzeit University — Faculty of Engineering and Technology
**Instructors:** Dr. Ahmad Afaneh, Dr. Hanna Bullata

---

## Overview

A multi-process simulation of a home furnishing competition between two teams.
Each team has a chain of members that pass furniture pieces from a source member
to a sink member. The sink checks whether pieces arrive in sorted order.
The first team to win the required number of rounds wins the competition.

The project demonstrates the use of **pipes**, **signals**, **fork/exec**,
**OpenMP** parallel processing, and **OpenGL** visualization under Linux.

---

## How It Works

- Two teams (A and B) compete simultaneously
- Each team has N members arranged in a chain: Source → Middle(s) → Sink
- The source picks a piece and sends it forward through the chain
- The sink checks if pieces arrive in the correct sorted order
- If a piece arrives out of order it is bounced back to the source
- When all pieces are placed correctly the team wins that round
- The competition runs until one team reaches the required number of wins

### Member Roles

| Role | Position | Job |
|------|----------|-----|
| Source | First member | Picks pieces, sends forward, handles bounced pieces |
| Middle | Between source and sink | Relays pieces forward and backward |
| Sink | Last member | Checks sorted order, accepts or bounces |

### Fatigue System
Each member slows down over time. Delay increases by 10% for every 10 pieces moved:
```
delay = base_delay × (1 + 0.10 × pieces_moved / 10)
```

---

## Project Structure

```
furnishing/
├── main.c          Main process — generates pieces, forks teams, tracks wins
├── member.c        Member process logic (source, middle, sink roles)
├── member.h        Member argument struct and function declaration
├── simulation.h    Shared constants, message struct, pipe helpers
├── config.c        Config file reader
├── config.h        Config struct definition
├── display.c       OpenGL visualization (animated teams and pieces)
├── display.h       Display function declarations
├── config.txt      Configuration file (edit this to change behaviour)
└── Makefile        Build instructions
```

---

## IPC Used

| Mechanism | Purpose |
|-----------|---------|
| **Pipes** | Each pair of adjacent members communicates through a pair of pipes (forward + backward) |
| **Signals** | SIGTERM used for clean shutdown of all child processes |
| **OpenMP** | Generates piece IDs for both teams in parallel using `#pragma omp parallel for` |

### Message Types Through Pipes

| Message | Meaning | Direction |
|---------|---------|-----------|
| `MSG_FORWARD` | Piece moving toward sink | Source → Sink |
| `MSG_BOUNCE` | Piece rejected, sent back | Sink → Source |
| `MSG_SUCCESS` | Piece accepted | Sink → Source (notify) |
| `MSG_DONE` | All pieces placed, team wins | Sink → Parent |

---

## Configuration File (`config.txt`)

```
num_members  = 4      # members per team (min 2: one source + one sink)
num_pieces   = 15     # furniture pieces to move per round
min_delay_ms = 50     # minimum delay per member (milliseconds)
max_delay_ms = 250    # maximum delay per member (milliseconds)
wins_needed  = 2      # rounds a team must win to end the competition
random_ids   = 1      # 1=random piece IDs, 0=sequential 1..N
```

---

## How to Build and Run

### Install dependencies
```bash
sudo apt-get install -y gcc make freeglut3-dev
```

### Build
```bash
make
```

### Run
```bash
./furnishing config.txt
```

### Clean
```bash
make clean
```

---

## OpenGL Display

The OpenGL window shows:
- Two team lanes side by side (Team A left, Team B right)
- Animated pieces moving along the chain
- Current round number and score
- Win/lose status for each round

---

## Process Structure

```
main (parent)
├── OpenMP: generates pieces for Team A and Team B in parallel
├── fork → Team A: Source → Middle(s) → Sink  (connected by pipes)
└── fork → Team B: Source → Middle(s) → Sink  (connected by pipes)

Parent uses select() to wait for MSG_DONE from either team
```

---

## Example Output

```
Round 1 starting...
[Team A] Source sending piece 7
[Team B] Source sending piece 3
[Team A] Sink accepted piece 7 (1/15)
[Team B] Sink bounced piece 3 — out of order
...
[Team A] Round 1 WINS!
Score: A=1  B=0
```
