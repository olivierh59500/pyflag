# start pyflag, very simple for now
export PYTHONPATH=`pwd`:`pwd`/libs/
# Add our libs dir to the LD_LIBRARY_PATH to run our hooker.
export LD_LIBRARY_PATH=`pwd`/libs/

epydoc --html -o docs/ -n Flag -c default --inheritance grouped  `find pyflag/ -name \*.py` `find plugins/ -name \*.py`
