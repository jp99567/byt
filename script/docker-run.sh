
REPO=$(git rev-parse --show-toplevel)
git describe --tags --always --dirty

#--entrypoint /bin/bash -it \
docker run -it \
 --volume $REPO:/repo \
 --name="bytd_for_bb_on_deb13" \
bytd_bb_deb13

