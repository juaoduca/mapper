
# ecm Project - ORM FastCGI Module with Visitor Pattern

## Structure

- `main.cpp`: Program entry, demo for DDL generation via Visitor pattern.
- `include/orm.hpp`, `include/visitor.hpp`, `src/orm.cpp`, `src/visitor.cpp`: Clean OO code.
- `data/example-schema.json`: Example JSON schema for a table/model.

## Dependencies

- nlohmann-json3-dev (for JSON parsing)
    sudo apt install nlohmann-json3-dev

## Build

```sh
mkdir build && cd build
cmake ..
make
```

## Run

```sh
./orm
```
