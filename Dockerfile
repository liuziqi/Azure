FROM ubuntu
ADD bin/my_http_server bin/http_server
ADD lib/libazure.so lib/libazure.so
ADD lib/libyaml-cpp.so lib/libyaml-cpp.so
ADD lib/libyaml-cpp.so.0.6 lib/libyaml-cpp.so.0.6
ENV LD_LIBRARY lib
EXPOSE 8020
ENTRYPOINT ["bin/http_server"]

# docker run -id -v [source directory]:[target directory] [name/id] --name=name 
# docker build -f [file_path] -t [name] .
# docker run -it [name]
# docker stop [name]
# docker rmi -f [imageid]
# docker images
# docker container ls -a
# docker container ls