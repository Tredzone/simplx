# Compilation
make DEBUG=[0 or 1] V=[1 or 2]

V1 =>  shows usage of pipes between two threads on differents cores (preferably isolated) : communication loops use all core's ressources but doesn't bother

V2 =>  shows usage of pipes between two threads on same (or not managed) core : communication loops needs to be interrupted (thread yield) to acheive each message passthrough

V3 =>  shows integration of (even not actor model) other systems with simplx through thoses pipes 

