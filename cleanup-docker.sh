docker image ls | grep odyssey | awk '{print $3}' | xargs docker image rm -f || true
docker rm $(docker stop $(docker ps -aq)) || true
docker network rm $(docker network ls | grep jepsen | egrep -o '^[^ ]+') || true