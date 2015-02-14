#ifndef MULTIPLE_TASK_HANDLER_HPP
#define MULTIPLE_TASK_HANDLER_HPP
/*
multiple_task_handler.hpp
2014/09/19
psycommando@gmail.com
Description: This is basically a system for handling multiple tasks in parallel.
             something close to a threadpool. 

             #TODO: Need testing.
*/
#include <thread>
#include <mutex>
#include <future>
#include <functional>
#include <deque>
#include <atomic>

namespace multitask
{
    //Typedefs
    typedef bool                              pktaskret_t; 
    typedef std::packaged_task<pktaskret_t()> pktask_t;

    /*
        A container to contain the state of tasks assigned to threads.
    */
    //struct runTaskContainer
    //{
    //    uint32_t                threadWaitSchedule; //This sets the waiting time a thread waits before checking for a new task
    //    std::mutex              taskmutex;          //Just in case the compiler plays us a prank...
    //    std::atomic<bool>       taskAvailable;      //This is set to false when the task is moved, and to true when a new task is moved over it!

    //    std::mutex              newTaskmutex;       //Mutex for the condition var below
    //    std::condition_variable newTask;            //This is triggered when a new task is set

    //    pktask_t                task;
    //};

    /*
        CMultiTaskHandler
            Class meant to manage a task queue and a small threadpool. It process tasks in parallel, and provide means to 
            wait for the end of the execution, or stop it/pause it.
    */
    class CMultiTaskHandler
    {
    public:
        CMultiTaskHandler();
        ~CMultiTaskHandler();

        //Add task
        void AddTask( pktask_t && task ); 

        //This waits until all tasks have been processed before returning!
        void BlockUntilTaskQueueEmpty();

        //Start the manager's thread and begins handling tasks.
        // If the thread is running, it does nothing.
        void Execute();

        //Stops the execution of the thread after the current task is completed
        // If the thread is stopped, it does nothing.
        void StopExecute();

        //Returns whether there are still tasks to run in the queue
        bool HasTasksToRun()const;

    private:

        struct thRunParam
        {
            std::atomic<bool>        runningTask;  //Whether the thread is running a task or not
            std::chrono::nanoseconds waitTime;     //the time this thread will wait in-between checks for a new task
        };

        //Methods
        void RunTasks();
        //void AssignTasks( std::vector< std::pair<std::future<pktaskret_t>,std::thread> > & futthreads );
        bool WorkerThread( thRunParam & taskSlot );

        //Don't let anynone copy or move construct us
        CMultiTaskHandler( const CMultiTaskHandler & );
        CMultiTaskHandler( CMultiTaskHandler && );
        CMultiTaskHandler& operator=(const CMultiTaskHandler&);
        CMultiTaskHandler& operator=(CMultiTaskHandler&&);

        //Variables
        std::thread                                  m_managerThread;

        std::mutex                                   m_mutextasks;
        std::deque<pktask_t>                         m_tasks;

        std::mutex                                   m_mutexTaskFinished;
        std::condition_variable                      m_lastTaskFinished;

        std::mutex                                   m_mutexNewTask;
        std::condition_variable                      m_newTask;

        std::atomic_bool                             m_NoTasksYet; //This is to avoid blocking 
                                                                   //on the condition variable in 
                                                                   //case of wait on an empty task queue after init
        std::atomic_bool                             m_managerShouldStopAftCurTask;
        std::atomic_bool                             m_stopWorkers;

        std::atomic<int>                             m_taskcompleted;
    };
};
#endif