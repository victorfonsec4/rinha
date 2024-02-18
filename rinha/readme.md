* cpp server *
$bazel run --copt=-O2 -- //rinha:main
$chmod 777 /tmp/unix_sockets_example.sock

*nginx docker*

$sudo docker build -t rinha-nginx -f nginx.dockerfile .

$sudo docker run -p 9999:9999 -v /tmp:/tmp rinha-nginx
