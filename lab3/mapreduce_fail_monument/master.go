package mapreduce
import "container/list"
import "fmt"

type WorkerInfo struct {
    address string
    // You can add definitions here.
}


// Clean up all workers by sending a Shutdown RPC to each one of them Collect
// the number of jobs each work has performed.
func (mr *MapReduce) KillWorkers() *list.List {
    l := list.New()
    mr.mux.Lock()
    for _, w := range mr.Workers {
        DPrintf("DoWork: shutdown %s\n", w.address)
        args := &ShutdownArgs{}
        var reply ShutdownReply;
        ok := call(w.address, "Worker.Shutdown", args, &reply)
        if ok == false {
            fmt.Printf("DoWork: RPC %s shutdown error\n", w.address)
        } else {
            l.PushBack(reply.Njobs)
        }
    }
	mr.mux.Lock()
    return l
}

func (mr *MapReduce) pollWorker() string {
    for {
        fmt.Printf("in poll worker")
		var cname string
		cname = <-mr.registerChannel
		mr.mux.Lock()
		mr.Workers[cname] = &WorkerInfo{}
		mr.Workers[cname].address = cname
        mr.mux.Unlock()

        /*
        var worker string
        worker = <-mr.registerChannel
        mr.Workers[worker] = &WorkerInfo{}
        mr.Workers[worker].address = worker
        fmt.Printf("Love & Peace %s", worker)
        */
    }
}

func (mr *MapReduce) monitorWorker(worker *WorkerInfo, args *DoJobArgs, reply *DoJobReply) {
    ok := call(worker.address, "Worker.DoJob", args, &reply)
    if ok {
        mr.workerChannel <- -1
    } else {
        mr.workerChannel <- args.JobNumber
    }
}

func (mr *MapReduce) GetWorker() *WorkerInfo {
    for {
        for key, value := range mr.Workers {
		    mr.mux.Lock()
            delete(mr.Workers, key)
            return value;
		    mr.mux.Unlock()
        }
    }
}

func (mr *MapReduce) RunMaster() *list.List {
    // Your code here
    go mr.pollWorker()

    i := 0
    for i < mr.nMap {
        args := &DoJobArgs{mr.file, Map, i, mr.nReduce}
        var reply DoJobReply
        go mr.monitorWorker(mr.GetWorker(), args, &reply)
        i++
        fmt.Printf("1: %d", i)
    }

    i = 0
    for i < mr.nMap{
        workerStatus := <-mr.workerChannel
        if workerStatus == -1 {
            i++
        } else {
            args := &DoJobArgs{mr.file, Map, workerStatus, mr.nReduce}
            var reply DoJobReply
            go mr.monitorWorker(mr.GetWorker(), args, &reply)
        }
        fmt.Printf("2: %d", i)
    }

    i = 0
    for i < mr.nReduce {
        args := &DoJobArgs{mr.file, Reduce, i, mr.nMap}
        var reply DoJobReply
        go mr.monitorWorker(mr.GetWorker(), args, &reply)
        i++
        fmt.Printf("3: %d", i)
    }

    i = 0
    for i < mr.nMap{
        workerStatus := <-mr.workerChannel
        if workerStatus == -1 {
            i++
        } else {
            args := &DoJobArgs{mr.file, Reduce, i, mr.nMap}
            var reply DoJobReply
            go mr.monitorWorker(mr.GetWorker(), args, &reply)
        }
        fmt.Printf("4: %d", i)
    }
    return mr.KillWorkers()
}
