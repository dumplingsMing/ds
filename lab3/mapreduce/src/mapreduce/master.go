package mapreduce

import "container/list"
import "fmt"

type WorkerInfo struct {
	address string
	idle    bool
	// You can add definitions here.
}

// Clean up all workers by sending a Shutdown RPC to each one of them Collect
// the number of jobs each work has performed.
func (mr *MapReduce) KillWorkers() *list.List {
	l := list.New()
	for _, w := range mr.Workers {
		DPrintf("DoWork: shutdown %s\n", w.address)
		args := &ShutdownArgs{}
		var reply ShutdownReply
		ok := call(w.address, "Worker.Shutdown", args, &reply)
		if ok == false {
			fmt.Printf("DoWork: RPC %s shutdown error\n", w.address)
		} else {
			l.PushBack(reply.Njobs)
		}
	}
	return l
}
func (mr *MapReduce) addWorker() string {
	for {
        worker := <-mr.registerChannel
		mr.mux.Lock()
		mr.Workers[worker] = &WorkerInfo{worker, true}
		mr.mux.Unlock()
	}
}

func (mr *MapReduce) GetWorker() *WorkerInfo {
    for {
        mr.mux.Lock()
        for _, value := range mr.Workers {
            if value.idle {
                value.idle = false
                mr.mux.Unlock()
                return value
            }
        }
        mr.mux.Unlock()
    }
}

func (mr *MapReduce) RunMaster() *list.List {
	// Your code here
	go mr.addWorker()
	for i := 0; i < mr.nMap; {
        worker :=  mr.GetWorker()
		args := &DoJobArgs{mr.file, Map, i, mr.nReduce}
		var reply DoJobReply
		ok := call(worker.address, "Worker.DoJob", args, &reply)
		if ok == false {
			mr.mux.Lock()
			delete(mr.Workers, worker.address)
			mr.mux.Unlock()
			continue
		}
		mr.mux.Lock()
		worker.idle = true
		mr.mux.Unlock()
		i++
	}

	for i := 0; i < mr.nReduce; {
        worker :=  mr.GetWorker()
		args := &DoJobArgs{mr.file, Reduce, i, mr.nMap}
		var reply DoJobReply
		ok := call(worker.address, "Worker.DoJob", args, &reply)
		if ok == false {
			mr.mux.Lock()
			delete(mr.Workers, worker.address)
			mr.mux.Unlock()
			continue
		}
		mr.mux.Lock()
        worker.idle = true
		mr.mux.Unlock()
		i++
	}
	return mr.KillWorkers()
}
