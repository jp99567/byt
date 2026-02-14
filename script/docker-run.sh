
REPO=$(git rev-parse --show-toplevel)
git describe --tags --always --dirty

docker run -it \
 --volume $REPO:/repo \
bytd_bb_deb13

# --entrypoint bash --name="bytd_for_bb_on_deb13_bash" \
