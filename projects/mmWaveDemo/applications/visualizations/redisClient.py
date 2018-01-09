import redis

r = redis.StrictRedis(host = "localhost", port=6379,db=0)
buf = ""
while True:

    buf = r.lpop("radar")
    print buf
    a = 1
