# ISEC - Operating Systems (SO) - Topic-Based Messaging Platform (2024/2025)

## Context

This repository contains the project materials developed for the practical assignment of the **Operating Systems (SO)** course unit, part of the Computer Engineering degree at the Coimbra Institute of Engineering (ISEC), during the 2024/2025 academic year.

## Project Goal

The primary objective was to implement a **topic-based messaging platform** in **C** using **UNIX System Programming** concepts. The system consists of a central `manager` server and multiple `feed` clients, enabling users to subscribe to topics and exchange persistent or non-persistent messages.

## Key Aspects Covered

* **Architecture:** Client-Server model with a central `manager` process handling logic and multiple `feed` client processes for user interaction.
* **Inter-Process Communication (IPC):** Communication relies exclusively on **Named Pipes (FIFOs)**. The `manager` uses a main pipe to receive requests, and each `feed` client creates a unique pipe to receive messages from the manager. System calls like `read()` and `write()` were prioritized.
* **Concurrency (`feed` Client):** **Threads** are used in the `feed` client to handle simultaneous tasks: one thread receives messages from the manager, while the main thread processes user commands.
* **Concurrency & Synchronization (`manager` Server):** **Threads** manage manager commands, incoming client messages, and persistent message timeouts. **Mutexes** protect shared resources (user lists, topic data, persistent messages) during concurrent access.
* **Signaling:** Used for graceful shutdown and notifications. **Signals** (e.g., `SIGUSR1`) notify client threads to terminate when the manager removes a user or shuts down.
* **Messaging System Logic:**
    * User authentication based on unique usernames.
    * Dynamic topic creation and subscription/unsubscription.
    * Broadcasting messages to subscribed users.
    * Handling of **persistent messages** with a defined lifetime (saved to/loaded from file) and non-persistent messages.
* **Manager Administration:** Commands for listing/removing users, listing/showing/locking/unlocking topics, and shutting down the platform gracefully.
* **File Persistence:** Persistent messages are saved to a text file (specified by `MSG_FICH` environment variable) upon manager shutdown and reloaded on startup.

## Tools and Languages Used

* **Language:** C
* **OS:** Linux (Debian)
* **IDE:** VS Code

## Repository Contents

| File / Folder                     | Description                                                                                                                              |
| :-------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------- |
| **`/Sources/`** | **Contains all C source code** (`feed`, `manager`, `plataforma_mensagens.h`), the `makefile`, and potentially IDE configuration folders (`.vscode`). |
| `so_2425_tp_DiogoSilva_2023139070_JoãoPomar_2023140947.pdf` | The final project report detailing the architecture, implementation choices, and challenges.                                |
| `SO - 2425 - Enunciado Trabalho Pratico.pdf` | The original assignment brief (Enunciado) for the course unit.                                                            |

## Authors

* Diogo Marques Silva
* João Pedro Vila Pomar
