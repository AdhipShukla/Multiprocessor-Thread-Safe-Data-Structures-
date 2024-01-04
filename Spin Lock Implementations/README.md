[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-24ddc0f5d75046c5622901739e7c5dd533143b0c8e959d652212380cedb1ea36.svg)](https://classroom.github.com/a/eubJUoYg)
[![Open in Visual Studio Code](https://classroom.github.com/assets/open-in-vscode-718a45dd9cf7e7f842a935f5ebbe5719a5e09af4491e668f4dbf3b35d5cca122.svg)](https://classroom.github.com/online_ide?assignment_repo_id=11986599&assignment_repo_type=AssignmentRepo)
**Note: This README is perpetually under construction; please check back frequently.**

The objective of this assignment is to implement mutual exclusion locks that are efficient and scalable.

**Logistics**

You need access to the Computer Science (CS) rlogin server cluster for this assignment. You need  CS credentials to access the system (by performing an ssh to rlogin.cs.vt.edu). If you are a Computer Science student, you should already have CS credentials. If you are an ECE student,  you should create an account by following: [following this link](https://admin.cs.vt.edu/create.pl). **The username will match the user's Virginia Tech username exactly. The password can and should be different from the user's Virginia Tech password.** Password policies are available here: [Password Requirements](https://wiki.cs.vt.edu/index.php/Password_Requirements).

You will submit the assignment using the course [GitHub Classroom Repos](https://classroom.github.com/classrooms/142837312-virginia-tech-cs-systems-courses-classroom-5510). Your instructor will provide you with a link to access your repo and submit the assignment. Your repository should include everything the assignment asks of you, including the code, report with correctness and performance measurements (in markup language), and a comprehensive README file that will allow us to quickly compile, run, test, and experimentally evaluate your code.

We have decided to move the source code language from Java to C for this assignment. We considered recent student feedback and the fact that writing the code in C allows you to learn and understand better the semantics of hardware instructions for synchronization, as well as their implications on the safety, liveness, and performance properties of synchronization algorithms.

**Objectives**

We provide you with three implementations of spin locks. The first implementation uses the native POSIX threads library mutual exclusion lock. The second and third implementations use a basic spinlock using a test-and-set operation, which is implemented in inline x86 Assembly using the xchg (second implementation) and cmpxchg (third implementation) instructions, respectively.

We are asking you to provide four additional lock implementations:

1. An improved version of the test-and-set spin lock that uses backoff, also known as test-and-test-and-set lock.  **(25% of your grade).**

2. A queue lock known as MCS --MCS stands for John Mellor-Crummery and Michael Scott, the two researchers who invented this algorithm.  **(30% of your grade)**.

3. An alternative queue lock implementation, CLH -- where CLH is for Craig, Landin, and Hagersten.  **(30% of your grade).**

4. The "One Lock that Rules them All," maybe. This is a lock implementation entirely of your choice. An excellent place to start is to tune the lock algorithms mentioned above, e.g., by carefully selecting the parameters or introducing some notion of core hierarchy or topology in the acquiring and releasing logic to better match the architecture and core topology of the target system. The target system is a two-socket, 32-core, 64-hyperthread NUMA system in the case of rlogin. **(5% of your grade)**.


We also ask you to provide a report with your lock performance measurements as a markup file with this directory **(10% of your grade)**.

We purposely assign a low weight (5%) to the open-ended task of finding a better-performing lock. We want to ensure that you all get a high grade for this assignment by correctly implementing tried and known solutions before you attempt one of your own.

The repository comes with a test to measure the performance and check the correctness of your locks. The test checks the correctness and measures the performance of the locks on up to 32 cores. You will run all your experiments on the Computer Science Department rlogin cluster servers. 

The code includes an experimental function that lets you bind threads to cores. Many researchers who studied locks in the past found that binding threads to cores leads to more stable performance. However, we do not necessarily support this view. The servers are multi-user, multi-tasking machines, meaning that cores may be (heavily) time-shared between your experiments and other tasks. We deliberately do not act against this behavior because we want you to experience the implications of processor sharing on synchronization performance. Therefore, we ask you to experiment with and without thread binding to cores and draw your own conclusions.

Later on in the programming assignments, we will experiment with algorithms that are more robust to processor sharing than the spin locks studied in this assignment. 

**References**

Chapter 7 of your textbook covers the spinlock algorithms we consider in this assignment, including references to seminal papers describing the most successful and widely used algorithms. We particularly wish to draw your attention to the classic paper of John Mellor-Crummey and Michael Scott (reference [124], work that has also earned the authors the Edsger W. Dijkstra Prize in Distributed Computing), an earlier classic work by Larry Rudolph and Zary Sgeall on the TTAS lock (reference [150]), and an also classic but largely overlooked technical report by Travis Craig on queue lock s(references [32]). References [37], [121], and [144] might be good starting points for thinking about how to implement more efficient locks.  

~                                        
~                                        

