import "./calc.bi" as calc

print("")
print("  bi calculator")
print("  ─────────────")
print("  try:  (2 + 3) * 4")
print("  type 'quit' or Ctrl-D to exit")
print("")

while (true) {
    let line = input("  > ")
    if (line == null) { break }
    line = trim(line)
    if (line == "")     { continue }
    if (line == "quit") { break }
    if (line == "exit") { break }
    if (line == "q")    { break }
    try {
        let result = calc.calc(line)
        print("  = " + str(result))
        print("")
    } catch (e) {
        print("  error: " + str(e))
        print("")
    }
}

print("")
print("  bye")
print("")
