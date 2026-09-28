import "./cal.bi" as calc

fn banner() {
    print("")
    print("  bi calculator")
    print("  ─────────────")
    print("  try:  (2 + 3) * 4")
    print("  type 'quit' or Ctrl-D to exit")
    print("")
}

fn run() {
    banner()

    while (true) {
        let line = input("  > ")
        if (line == null) { break }

        line = trim(line)

        if (line == "")          { continue }
        if (line == "quit")      { break }
        if (line == "exit")      { break }
        if (line == "q")         { break }

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
}

run()