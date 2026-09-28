// Calculator core — pure logic

fn tokenize(s) {
    let tokens = []
    let i = 0
    let n = len(s)
    while (i < n) {
        let c = s.charAt(i)
        if (c == " " || c == "\t") { i += 1; continue }
        if (c >= "0" && c <= "9") {
            let num = ""
            let seenDot = false
            while (i < n) {
                let d = s.charAt(i)
                if (d >= "0" && d <= "9") { num += d; i += 1 }
                else if (d == "." && !seenDot) { num += d; seenDot = true; i += 1 }
                else { break }
            }
            tokens.push({ type: "num", value: num })
            continue
        }
        if (c == "+" || c == "-" || c == "*" || c == "/" ||
            c == "%" || c == "(" || c == ")") {
            tokens.push({ type: "op", value: c })
            i += 1
            continue
        }
        throw "unexpected character: " + c
    }
    return tokens
}

fn evaluate(tokens) {
    let pos = 0
    let n = len(tokens)

    fn peek() {
        if (pos >= n) { return null }
        return tokens[pos]
    }
    fn advance() {
        let t = tokens[pos]
        pos += 1
        return t
    }
    fn parseExpr() {
        let left = parseTerm()
        while (true) {
            let t = peek()
            if (t == null) { break }
            if (t.type != "op") { break }
            if (t.value != "+" && t.value != "-") { break }
            advance()
            let right = parseTerm()
            if (t.value == "+") { left = left + right }
            else                { left = left - right }
        }
        return left
    }
    fn parseTerm() {
        let left = parseFactor()
        while (true) {
            let t = peek()
            if (t == null) { break }
            if (t.type != "op") { break }
            if (t.value != "*" && t.value != "/" && t.value != "%") { break }
            advance()
            let right = parseFactor()
            if (t.value == "*")      { left = left * right }
            else if (t.value == "/") {
                if (right == 0) { throw "division by zero" }
                left = left / right
            } else {
                if (right == 0) { throw "modulo by zero" }
                left = left % right
            }
        }
        return left
    }
    fn parseFactor() {
        let t = peek()
        if (t == null) { throw "unexpected end of expression" }
        if (t.type == "op" && t.value == "-") {
            advance()
            return -parseFactor()
        }
        if (t.type == "op" && t.value == "(") {
            advance()
            let v = parseExpr()
            let close = peek()
            if (close == null || close.value != ")") { throw "expected ')'" }
            advance()
            return v
        }
        if (t.type == "num") {
            advance()
            return num(t.value)
        }
        throw "unexpected token: " + t.value
    }

    let result = parseExpr()
    if (pos < n) { throw "extra tokens after expression" }
    return result
}

export fn calc(expr) {
    let trimmed = trim(expr)
    if (len(trimmed) == 0) { throw "empty expression" }
    let tokens = tokenize(trimmed)
    return evaluate(tokens)
}
