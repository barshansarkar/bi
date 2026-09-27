route GET "/" {
  return "<h1>Hello from .bi</h1>"
}

route GET "/users/:id" {
  return json({ id: params.id, name: "Ada" })
}

route GET "/api/time" {
  return json({ now: time() })
}

serve(8080)
