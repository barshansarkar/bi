// examples/app.bi — demo .bi web app
// run with:  bi serve examples/app.bi

let hits = 0

fn greet(name) {
  return "Hello, " + name + "!"
}

route GET "/" {
  hits = hits + 1
  return `
    <!doctype html>
    <html>
      <body style="font-family: system-ui; max-width: 640px; margin: 4rem auto">
        <h1>${greet("world")}</h1>
        <p>This page has been viewed <b>${hits}</b> times.</p>
        <ul>
          <li><a href="/api/info">/api/info</a></li>
          <li><a href="/user/42">/user/42</a></li>
          <li><a href="/echo?msg=hi">/echo?msg=hi</a></li>
        </ul>
      </body>
    </html>
  `
}

route GET "/api/info" {
  return json({
    name: "bi-demo",
    version: "0.1.0",
    uptime: time(),
    routes: ["/", "/api/info", "/user/:id", "/echo"]
  })
}

route GET "/user/:id" {
  return json({ id: num(params.id), name: "User " + params.id })
}

route GET "/echo" {
  let msg = query.msg
  if (msg == null) { msg = "(nothing)" }
  return json({ youSaid: msg })
}

route POST "/echo" {
  return json({ received: body, length: len(body) })
}

route GET "/boom" {
  status(500)
  return json({ error: "something broke" })
}

route GET "/old" {
  redirect("/")
}

print("boot: " + str(time()))
serve(8080)