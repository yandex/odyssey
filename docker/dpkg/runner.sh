#!/usr/bin/env bash

set -ex

codename="bionic"
output_folder="packages"
while getopts ":hc:o:" option; do
    case $option in
        c)
            codename=$OPTARG;;
        o)
            output_folder=$OPTARG;;
        h)
            cat << EOM
$(basename "$0") [-h] [-c codename] [-o output folder] -- build odyssey deb in docker and save packages on host
where:
    -h  show this help text
    -c  codename of ubuntu distro (bionic, jammy, etc)
    -o  output folder
EOM
            exit 1;;
        \?)
            echo "Error: Invalid option"
            exit 1;;
   esac
done

mkdir -p "$output_folder"
image_name="odyssey/dpkg-$output_folder"
container_name="odyssey-packages-$codename"
docker build . --tag $image_name -f "./docker/dpkg/Dockerfile.$codename"
docker rm -f $container_name || true
docker create --name "$container_name" "$image_name"
docker cp $container_name:/packages $output_folder
docker rm -f $container_name
