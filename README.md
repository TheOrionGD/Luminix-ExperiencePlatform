## **ASCII Logo**

```text
  _                 _       _      
 | |               (_)     (_)     
 | |_   _ _ __ ___  _ _ __  ___  __
 | | | | | '_ ` _ \| | '_ \| \ \/ /
 | | |_| | | | | | | | | | | |>  < 
 |_|\__,_|_| |_| |_|_|_| |_|_/_/\_\
          
          luminix DB                        
                                   
```

---

# **Luminix** – Enterprise-Grade In-Memory JSON Database in C

**Luminix** is a **high-performance, modular, in-memory database** built in **C**, designed for **low-latency operations, persistent storage, and extensible architecture**. It leverages **JSON for data interchange**, enabling seamless integration with other applications and systems.

Luminix is ideal for developers, researchers, and engineers looking to **understand database internals, memory management, and scalable system design** in a low-level language.

---

## **Table of Contents**

1. [Overview](#overview)
2. [Key Features](#key-features)
3. [Architecture & Modules](#architecture--modules)
4. [Data Model & Structures](#data-model--structures)
5. [Tech Stack](#tech-stack)
6. [Installation & Build](#installation--build)
7. [Usage & CLI](#usage--cli)
8. [Example Queries & JSON Operations](#example-queries--json-operations)
9. [Advanced Extensions](#advanced-extensions)
10. [Contributing & Governance](#contributing--governance)
11. [License](#license)

---

## **Overview**

Luminix provides:

* **Fast in-memory operations** – All records reside in memory for minimal latency
* **Persistent JSON storage** – Ensures human-readable, portable, and interoperable data
* **Modular architecture** – Each component can be independently upgraded or replaced
* **Indexing & search optimization** – Efficient hash-based or tree-based lookups
* **Enterprise-grade extensibility** – Designed for multi-table support, concurrency, and networking

Luminix combines **low-level C performance** with **modern JSON interoperability**, bridging traditional system programming with contemporary application requirements.

---

## **Key Features**

| Feature            | Description                                                      |
| ------------------ | ---------------------------------------------------------------- |
| CRUD Operations    | Full Create, Read, Update, Delete support for structured records |
| JSON Persistence   | Store/load database state in `database.json` for portability     |
| In-Memory Indexing | Hash table or tree-based indexing for fast lookups               |
| Modular Design     | Separate modules for DB core, JSON I/O, indexing, and utilities  |
| CLI Interface      | Menu-driven, user-friendly command-line operations               |
| SQL-like Commands  | Familiar query syntax for CRUD operations                        |
| Memory Management  | Dynamic allocation and cleanup to prevent leaks                  |
| Extensibility      | Supports multi-table and multi-threaded future extensions        |

---

## **Architecture & Modules**

```
Luminix Core
├── main.c           # CLI entry point & orchestration
├── database.c/.h    # Core database engine: CRUD, memory management
├── json_io.c/.h     # JSON serialization/deserialization using cJSON
├── index.c/.h       # Indexing and search acceleration
├── utils.c/.h       # Helper functions, validation, parsing
├── config.h         # Global constants, file paths, system settings
└── tests/           # Unit and integration test scripts
```

**Design Principles:**

1. **Modularity:** Each component can be extended or replaced independently.
2. **Performance-first:** Memory-efficient structures and optimized algorithms.
3. **Extensible Persistence:** JSON allows integration with other systems or languages.
4. **Scalable Design:** Supports future multi-threaded, multi-table, or networked operations.

---

## **Data Model & Structures**

* **Record Struct:** Stores individual entries with flexible fields
* **Linked List / Dynamic Array:** In-memory storage of records
* **Index Structures:** Hash tables or trees for efficient search
* **JSON Mapping:** Each record serialized/deserialized as JSON object
* **File-Based Storage:** `database.json` maintains persistent state across sessions

**Data Flow:**

1. Insert/Update → Convert struct → JSON → Save file
2. Load → JSON → Parse → Map to in-memory structures
3. Search/Delete → In-memory operations → JSON sync

---

## **Tech Stack**

* **Programming Language:** C (C99/C11)
* **Persistence:** JSON (via [cJSON](https://github.com/DaveGamble/cJSON))
* **Data Structures:** Dynamic arrays, linked lists, hash tables, trees
* **Build & Compilation:** GCC / Clang, optional Makefile
* **Dev Tools:** VSCode/CLion, GDB, Valgrind, CMake for complex builds
* **Testing:** CLI-based unit tests, edge-case validation, JSON integrity checks
* **Future Extensions:** pthread/OpenMP for concurrency, TCP/IP networking, multi-table support

---

## **Installation & Build**

1. Clone repository:

```bash
git clone https://github.com/yourusername/luminix.git
cd luminix
```

2. Install **cJSON** library:

```bash
git clone https://github.com/DaveGamble/cJSON.git
cd cJSON
mkdir build && cd build
cmake ..
make
sudo make install
```

3. Compile project:

```bash
gcc -o luminix main.c database.c json_io.c index.c utils.c -lcjson
```

4. Run the program:

```bash
./luminix
```

---

## **Usage & CLI**

The CLI supports:

* Insert, read, update, delete records
* Save/load database to JSON
* Simple SQL-like query simulation

**Example Session:**

```text
Welcome to Luminix DB
1. Insert Record
2. Read/Search Records
3. Update Record
4. Delete Record
5. Save Database
6. Load Database
0. Exit
Enter your choice: 1
```

---

## **Example Queries & JSON Operations**

```text
INSERT INTO employees VALUES (201, "John Doe", "Engineering", 75000);
SELECT * FROM employees WHERE id = 201;
UPDATE employees SET salary = 80000 WHERE id = 201;
DELETE FROM employees WHERE id = 201;
```

* Each record maps directly to a **JSON object** in `database.json`

---

## **Advanced Extensions**

* **Multi-Table Support:** Handle multiple JSON tables with relationships
* **Advanced Indexing:** B-trees, AVL trees for large datasets
* **Full SQL Parser:** Support complex queries and filtering
* **Concurrency & Thread-Safety:** Multi-threaded read/write operations
* **Networking Module:** JSON-based client-server operations
* **Compression & Encryption:** Secure storage for sensitive enterprise data

---

## **Contributing & Governance**

* Fork the repo → create a branch → submit PR with description
* Follow **modular design principles** when extending
* Unit tests required for all new features

---

## **License**

MIT License © 2026 [Your Name]

---
