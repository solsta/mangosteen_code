#!/bin/bash

sleep 1
echo "Running cleanup script"

killall -9 redis kv_state_machine local_state_machine local_state_machine_multi_thread state_machine_rpc queue > /dev/null 2>&1

rm /home/vagrant/mangosteen_pmem_data/test_outputs/r* > /dev/null 2>&1 
rm /home/vagrant/mangosteen_pmem_data/test_outputs/maps/* > /dev/null 2>&1
