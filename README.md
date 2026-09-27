# .bi

A small web programming language with a built-in HTTP server and a pip-style package manager.

Written in C++17. No dependencies beyond the standard library and POSIX sockets. About 1,500 lines total.

```bi
route GET "/" {
  return "<h1>Hello, .bi!</h1>"
}

route GET "/api/time" {
  return json({ now: time() })
}

serve(8080)
```

Run it. You have a web server.

---

## Install

**Linux x86_64** — one line, no dependencies:

```bash
curl -fsSL https://github.com/barshansarkar/bi/raw/main/install.sh | bash
```

**From source** (any Linux, macOS):

```bash
git clone https://github.com/barshansarkar/bi
cd bi
make
sudo make install
```

Verify:

```bash
bi version
# bi 0.1.0
```

---

## Quick start

```bash
bi new myapp
cd myapp
bi serve src/main.bi
```

Open http://localhost:8080

---

## The language

### Variables

```bi
let name = "ada"
var count = 0
count = count + 1
```

### Types

- Numbers — `42`, `3.14`, `-1`
- Strings — `"hello"`, `'world'`
- Booleans — `true`, `false`
- Null — `null`
- Arrays — `[1, 2, 3]`
- Maps — `{ name: "ada", age: 36 }`
- Functions — first-class values

### Operators

```bi
a + b    a - b    a * b    a / b    a % b
a == b   a != b   a < b    a <= b   a > b   a >= b
a && b   a || b   !a
a += 1   a -= 1   a *= 2   a /= 2
```

### Control flow

```bi
if (x > 10) {
  print("big")
} else {
  print("small")
}

while (count < 10) {
  count += 1
}

for (let i = 0; i < 10; i += 1) {
  print(i)
}

for (let item in [1, 2, 3]) {
  print(item)
}

for (let key in { a: 1, b: 2 }) {
  print(key)
}
```

### Functions

```bi
fn add(a, b) {
  return a + b
}

let double = fn(x) { return x * 2 }

fn counter() {
  let n = 0
  return fn() { n = n + 1; return n }
}

let c = counter()
c()    // 1
c()    // 2
```

### Collections

```bi
let nums = [1, 2, 3]

nums.push(4)
nums.length
nums.map(fn(x) { return x * 2 })
nums.filter(fn(x) { return x > 2 })
nums.reduce(fn(acc, x) { return acc + x }, 0)

let user = { name: "ada", age: 36 }
user.name
user["age"]
user.email = "a@b.c"
```

### String methods

```bi
"hello".upper()
"hello".length
"a,b,c".split(",")
"hello".contains("ell")
"  hi  ".trim()
"hello".replace("l", "L")
"hello".substr(1, 3)
```

---

## Web

### Routes

```bi
route GET "/" {
  return "<h1>home</h1>"
}

route GET "/users/:id" {
  return "user " + params.id
}

route POST "/api/echo" {
  return json({ received: body })
}

route GET "/old" {
  redirect("/")
}
```

Methods: `GET`, `POST`, `PUT`, `DELETE`, `PATCH`.
Segments starting with `:` are captured into `params`.

### Request variables

| Variable  | Type   | Contents                                  |
|-----------|--------|-------------------------------------------|
| `req`     | map    | `{ method, path, body, headers, query }`  |
| `params`  | map    | `/users/:id` → `{ id: "42" }`             |
| `query`   | map    | `?name=ada` → `{ name: "ada" }`           |
| `body`    | string | raw request body                          |
| `method`  | string | `"GET"`, `"POST"`, …                      |
| `path`    | string | request path without query string         |

### Response helpers

```bi
return "raw text"                    // text/html
return json({ ok: true })            // application/json
return html("<h1>hi</h1>")           // explicit html

status(404)
header("X-Custom", "value")
redirect("/login")
```

### Starting the server

```bi
serve(8080)
```

Call once at the bottom of the file. Blocks until Ctrl+C.

### Complete example

```bi
let todos = []
let nextId = 1

route GET "/" {
  return "<h1>Todos</h1><p>See <a href='/api/todos'>/api/todos</a></p>"
}

route GET "/api/todos" {
  return json(todos)
}

route POST "/api/todos" {
  let text = query.text
  if (text == null) { text = "untitled" }
  let t = { id: nextId, text: text, done: false }
  nextId = nextId + 1
  todos.push(t)
  status(201)
  return json(t)
}

route GET "/api/todos/:id" {
  let id = num(params.id)
  for (let t in todos) {
    if (t.id == id) { return json(t) }
  }
  status(404)
  return json({ error: "not found" })
}

serve(8080)
```

Test:

```bash
curl -X POST 'http://localhost:8080/api/todos?text=buy+milk'
curl http://localhost:8080/api/todos
curl http://localhost:8080/api/todos/1
```

---

## Modules

```bi
// utils.bi
export fn greet(name) {
  return "Hello, " + name + "!"
}

export let VERSION = "1.0"
```

```bi
// main.bi
import "./utils.bi"

route GET "/" {
  return utils.greet("world")
}
```

With an alias:

```bi
import "./utils.bi" as u
u.greet("world")
```

---

## Packages

Install a package:

```bash
bi install router
```

Set the registry:

```bash
export BI_REGISTRY=https://packages.example.com
```

Other commands:

```bash
bi init                 # create bi.json in current folder
bi install              # install everything in bi.json
bi remove <pkg>         # uninstall
bi list                 # list installed packages
```

Use an installed package:

```bi
import "router"

route GET "/" {
  return router.name
}
```

---

## CLI reference

```
bi run <file.bi>          Run without starting a server
bi serve <file.bi>        Run and start the HTTP server
    --port <n>            Override the port passed to serve()
bi new <name>             Scaffold a new project directory
bi init                   Add bi.json + src/main.bi to current folder
bi install [pkg]          Install a package, or all from bi.json
bi remove <pkg>           Uninstall
bi list                   List installed packages
bi version                Print version
bi help                   Show this message
```

---

## Standard library

**Core** — `print`, `len`, `str`, `num`, `bool`, `type`, `assert`, `exit`

**Collections** — `range`, `map`, `filter`, `reduce`, `keys`, `values`

**Strings** — `split`, `join`, `trim`, `upper`, `lower`, `replace`

**Time / random** — `time`, `random`

**Files** — `readFile`, `writeFile`

**Web** — `json`, `html`, `status`, `header`, `redirect`, `serve`

---

## Project layout

```
myapp/
├── bi.json              manifest
├── src/
│   └── main.bi          entry point
└── bi_modules/          installed packages
    └── router/
        ├── bi.json
        └── src/main.bi
```

`bi.json`:

```json
{
  "name": "myapp",
  "version": "0.1.0",
  "entry": "src/main.bi",
  "dependencies": {
    "router": "*"
  }
}
```

---

## Building from source

```bash
git clone https://github.com/barshansarkar/bi
cd bi
make
sudo make install
```

Or with CMake:

```bash
cmake -B build
cmake --build build -j
sudo cmake --install build
```

### Source layout

```
src/
├── main.cpp            CLI entry point
├── token.hpp           token kinds
├── lexer.hpp           source to tokens
├── ast.hpp             AST node definitions
├── parser.hpp          tokens to AST
├── value.hpp           runtime values, JSON
├── builtins.hpp        native functions and methods
├── interpreter.hpp     tree-walking evaluator
├── http.hpp            HTTP/1.1 server
└── pkg.hpp             package manager
```

---

## Status

Early prototype. Version 0.1.0.

**Works:** everything documented above.

**Limitations:**

- No template literals yet — use `"..." + var`
- No HTTPS — put it behind nginx or Caddy
- No async I/O — thread-per-connection instead
- No GC — reference counting via `std::shared_ptr`
- Tree-walking interpreter — fine for prototypes, not for benchmarks
- Linux x86_64 binary only — macOS and ARM builds coming

---

## Contributing

Issues and pull requests welcome. If you report a bug, include:

- The `.bi` file that triggers it
- The exact command you ran
- The output you got

## License

MIT. See [LICENSE](LICENSE).
