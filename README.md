## Mangosteen instrumentation

### Prerequisites
 
1. Compile and build patched DynamoRIO
```
mkdir build
cmake ..
make -j
```

2. Install CRIU and start CRIU service

3. Configure persistent memory 
4. Create required directories on persistent memory:
```
mkdir /mnt/dax/test_outputs/mangosteen
mkdir /mnt/dax/test_outputs/mangosteen/maps/ 
```

4. Compile and build Mangosteen

```
mkdir build
cmake ..
```

5. Run one of the example applications.
```
#Clean up pmem directories
rm -rf /mnt/dax/test_outputs/mangosteen/r* && rm -rf /mnt/dax/test_outputs/mangosteen/maps/*
./examples/mangosteen_local/local_state_machine
```


6. Test recovery

Stop any dangling processes. If this is not done CRIU recovery will fail.
```
ps aux | grep state
kill ...
```

```
cd ~/criu/dump/snapshot/front_end
sudo criu restore -d -vvv -o restore.log --shell-job
```
