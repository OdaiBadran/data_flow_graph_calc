/* @author odai Badran */
/* @since 29/12/2020
/* Implementation for the Dataflow statistics calculator */

#include "dflow_calc.h"
#include <map>

using namespace std;

/**
 * class Data represents a mini data structure to hold three types of info
 * about instruction :
 * - time : represent dataflow dependency depth in clock cycles
 * - instTime : represent the latency of the instruction in clock cycles
 * - dependencies : array of size 2 to hold the dependencies instructions
 */
class Data {
public:
    int time;
    int InstTime;
    int* dependencies;
    Data() = default;
    ~Data(){
        delete dependencies;
    }
};

/**
 * class ProgAnalyzer represent the program analyzer which holds a map
 * (which is the graph) and also holds the number of instructions in
 * progTrace array.
 */
class ProgAnalyzer {
public:

    /** number of instructions in progTrace[] */
    int numOfInst_;

    /** map which represent the graph.
     * key = index of instruction, data = pointer to Data
     */
    map<int,Data*> graph_;

public:
    ProgAnalyzer() {
        graph_ = map<int,Data*>();
    }
    ~ProgAnalyzer() {
        map<int,Data*>::iterator it;
        for(it = graph_.begin(); it != graph_.end(); it++)
            delete it->second;
    }
    /** get a reference to the graph */
    map<int,Data*>& getGraph(){
        return graph_;
    }
    /** get the number of instructions in progTrace[] */
    int getNumOfInst(){
        return numOfInst_;
    }
    /** update the number of instruction to the given parameter */
    void setNumOfInst(int numOfInst) {
        numOfInst_ = numOfInst;
    }
};

/** get an array of size 2 of the dependencies of the give instruction */
static int* getDependencies(const InstInfo progTrace[], int instIndex) {
    int* dependencies = new int[2];
    dependencies[0] = -1;
    dependencies[1] = -1;
    bool src1IsFound = false;
    bool src2IsFound = false;
    for(int i = instIndex - 1; i >= 0; i--) {
        if(!src1IsFound) {
            if(progTrace[i].dstIdx == progTrace[instIndex].src1Idx) {
                dependencies[0] = i;
                src1IsFound = true;
            }
        }
        if(!src2IsFound) {
            if(progTrace[i].dstIdx == progTrace[instIndex].src2Idx) {
                dependencies[1] = i;
                src2IsFound = true;
            }
        }
        if(src1IsFound && src2IsFound) break;
    }
    return dependencies;
}

/** analyzeProg implementation */
ProgCtx analyzeProg(const unsigned int opsLatency[], const InstInfo progTrace[], unsigned int numOfInsts) {

    ProgAnalyzer* progAnalyzer = new ProgAnalyzer;
    progAnalyzer->setNumOfInst(numOfInsts);
    // fill the graph/map .
    // start from the last instruction in progTrace[] and find its dependencies.
    for(int i = numOfInsts - 1; i >= 0; i--) {
        int* dependencies = getDependencies(progTrace, i);
        Data* data = new Data;
        data->dependencies = dependencies;
        data->time = 0;
        data->InstTime = opsLatency[progTrace[i].opcode];
        progAnalyzer->getGraph().emplace(i, data);
    }
    // for each instruction in the graph fill the time in clock cycles
    // time <==> data flow depth (in clock cycles)
    for(int i = 0; i < numOfInsts; i++) {
        if (progAnalyzer->getGraph().operator[](i)->dependencies[0] == -1 &&
            progAnalyzer->getGraph().operator[](i)->dependencies[1] == -1) {
            progAnalyzer->getGraph().operator[](i)->time = 0;

        } else if (progAnalyzer->getGraph().operator[](i)->dependencies[0] == -1
                   && progAnalyzer->getGraph().operator[](i)->dependencies[1] !=
                      -1) {
            int depIndex = progAnalyzer->getGraph().operator[](
                    i)->dependencies[1];
            progAnalyzer->getGraph().operator[](i)->time =
                    progAnalyzer->getGraph().operator[](depIndex)->time +
                    opsLatency[progTrace[depIndex].opcode];
        } else if (progAnalyzer->getGraph().operator[](i)->dependencies[0] != -1
                   && progAnalyzer->getGraph().operator[](i)->dependencies[1] ==
                      -1) {
            int depIndex = progAnalyzer->getGraph().operator[](
                    i)->dependencies[0];
            progAnalyzer->getGraph().operator[](i)->time =
                    progAnalyzer->getGraph().operator[](depIndex)->time +
                    opsLatency[progTrace[depIndex].opcode];
        } else {
            int src1Index = progAnalyzer->getGraph().operator[](
                    i)->dependencies[0];
            int src2Index = progAnalyzer->getGraph().operator[](
                    i)->dependencies[1];
            int time1 = progAnalyzer->getGraph().operator[](src1Index)->time +
                        opsLatency[progTrace[src1Index].opcode];
            int time2 = progAnalyzer->getGraph().operator[](src2Index)->time +
                        opsLatency[progTrace[src2Index].opcode];
            progAnalyzer->getGraph().operator[](i)->time = max(time1, time2);
        }
    }

        return progAnalyzer;
}

void freeProgCtx(ProgCtx ctx) {
    ProgAnalyzer* ctxToFree = (ProgAnalyzer*)ctx;
    delete ctxToFree;
}

int getInstDepth(ProgCtx ctx, unsigned int theInst) {
    if(!ctx) return -1;
    ProgAnalyzer* progAnalyzer = (ProgAnalyzer*)ctx;
    if(theInst < 0 || theInst >= progAnalyzer->getNumOfInst()) return -1;
    return progAnalyzer->getGraph().operator[](theInst)->time;
}

int getInstDeps(ProgCtx ctx, unsigned int theInst, int *src1DepInst, int *src2DepInst) {
    if(!ctx) return -1;
    ProgAnalyzer* progAnalyzer = (ProgAnalyzer*)ctx;
    if(theInst < 0 || theInst >= progAnalyzer->getNumOfInst()) return -1;
    // the dependencies of the given instruction is already calculated/found :
    *src1DepInst = progAnalyzer->getGraph().operator[](theInst)->dependencies[0];
    *src2DepInst = progAnalyzer->getGraph().operator[](theInst)->dependencies[1];
    return 0;
}

int getProgDepth(ProgCtx ctx) {
    if(!ctx) return -1;
    ProgAnalyzer* progAnalyzer = (ProgAnalyzer*)ctx;
    map<int,Data*>::iterator it;
    int max_time = 0;
    // find the maximal data flow depth in clock cycles
    // simply iterate over the instructions in the graph and find
    // the one with maximal time(data flow depth in clock cycles)
    for(it = progAnalyzer->getGraph().begin(); it != progAnalyzer->getGraph().end();
    ++it) {
        int instTime = progAnalyzer->graph_.operator[](it->first)->InstTime;

        if(it->second->time + instTime > max_time)
            max_time = it->second->time + instTime;
    }
    return max_time;
}


