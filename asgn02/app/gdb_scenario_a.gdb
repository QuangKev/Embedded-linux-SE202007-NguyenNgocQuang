set pagination off
set confirm off
set print thread-events off
handle SIGUSR1 nostop noprint pass
define ringinfo
  echo \n##### STOP: thread, head/tail/size cua shared_buffer #####\n
  thread
  print cb->head
  print cb->tail
  print cb->size
  print cb->total_produced
end
break circular_buffer.c:92 if cb->capacity == 64
break circular_buffer.c:109 if cb->capacity == 64
run -D sim -i 100 -d 20 -l /tmp/sms_gdb.log -j /tmp/jitter_gdb.txt > /dev/null 2>&1
ringinfo
info threads
thread apply all bt
continue
ringinfo
continue
ringinfo
continue
ringinfo
echo \n##### scheduler-locking on: chi thread hien tai chay khi next #####\n
set scheduler-locking on
next
ringinfo
next
ringinfo
set scheduler-locking off
delete
continue
