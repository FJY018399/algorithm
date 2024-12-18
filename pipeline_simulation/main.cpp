#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

struct Instruction {
    enum Type { LOAD, STORE, ADD, SUB };
    Type type;
    std::string destReg;
    std::string op1;
    std::string op2;
    int ifCycle = -1;
    int idCycle = -1;
    int exCycle = -1;
    int memCycle = -1;
    int wbCycle = -1;
};

class PipelineSimulator {
private:
    std::vector<Instruction> instructions;

    bool isRegister(const std::string& op) {
        return op[0] == 'R';
    }

    // Check if inst2 depends on inst1
    bool hasDependency(const Instruction& inst1, const Instruction& inst2) {
        // LOAD/STORE special handling
        if (inst1.type == Instruction::LOAD) {
            // If inst1 is LOAD, inst2 can't use the loaded register until LOAD completes
            if (inst2.destReg == inst1.destReg ||
                (isRegister(inst2.op1) && inst2.op1 == inst1.destReg) ||
                (isRegister(inst2.op2) && inst2.op2 == inst1.destReg)) {
                return true;
            }
        } else if (inst1.type == Instruction::STORE) {
            // If inst1 is STORE, inst2 can't write to the same register
            if (inst2.destReg == inst1.destReg) {
                return true;
            }
        } else {
            // For arithmetic instructions (ADD/SUB), only check RAW dependencies
            if ((isRegister(inst2.op1) && inst2.op1 == inst1.destReg) ||
                (isRegister(inst2.op2) && inst2.op2 == inst1.destReg)) {
                return true;
            }
        }

        return false;
    }

public:
    void addInstruction(const std::string& line) {
        Instruction inst;
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "LOAD") {
            inst.type = Instruction::LOAD;
            ss >> inst.destReg;
            inst.destReg.pop_back();
            ss >> inst.op1;
        } else if (type == "STORE") {
            inst.type = Instruction::STORE;
            ss >> inst.destReg;
            inst.destReg.pop_back();
            ss >> inst.op1;
        } else if (type == "ADD") {
            inst.type = Instruction::ADD;
            ss >> inst.destReg;
            inst.destReg.pop_back();
            ss >> inst.op1;
            inst.op1.pop_back();
            ss >> inst.op2;
        } else if (type == "SUB") {
            inst.type = Instruction::SUB;
            ss >> inst.destReg;
            inst.destReg.pop_back();
            ss >> inst.op1;
            inst.op1.pop_back();
            ss >> inst.op2;
        }
        instructions.push_back(inst);
    }

    int simulate() {
        if (instructions.empty()) return 0;

        // Initialize first instruction
        instructions[0].ifCycle = 1;
        instructions[0].idCycle = 2;
        instructions[0].exCycle = 3;
        instructions[0].memCycle = 4;
        instructions[0].wbCycle = 5;

        // Process each instruction
        for (size_t i = 1; i < instructions.size(); i++) {
            Instruction& inst = instructions[i];
            const Instruction& prev = instructions[i-1];

            // Start IF one cycle after previous instruction's IF
            inst.ifCycle = prev.ifCycle + 1;

            // Initially set remaining stages with fixed spacing from previous instruction
            inst.idCycle = prev.idCycle + 1;
            inst.exCycle = prev.exCycle + 1;
            inst.memCycle = prev.memCycle + 1;
            inst.wbCycle = prev.wbCycle + 1;

            // Find dependencies and their types
            int maxDepCycle = inst.idCycle;  // Initialize to current ID cycle
            bool hasRegDep = false;
            bool hasLoadStoreDep = false;

            for (size_t j = 0; j < i; j++) {
                const Instruction& dep = instructions[j];
                if (hasDependency(dep, inst)) {
                    hasRegDep = true;
                    // All dependencies must wait for WB stage to complete
                    maxDepCycle = std::max(maxDepCycle, dep.wbCycle);
                    if (dep.type == Instruction::LOAD || dep.type == Instruction::STORE) {
                        hasLoadStoreDep = true;
                    }
                }
            }

            // Adjust stages based on dependencies
            if (hasRegDep) {
                inst.idCycle = maxDepCycle + 1;
                inst.exCycle = inst.idCycle + 1;
                inst.memCycle = inst.exCycle + 1;
                inst.wbCycle = inst.memCycle + 1;
            }

            // Special handling for LOAD/STORE instructions
            if (inst.type == Instruction::LOAD || inst.type == Instruction::STORE) {
                if (hasLoadStoreDep || hasRegDep) {
                    // Only wait for WB if there are actual dependencies
                    inst.idCycle = std::max(inst.idCycle, prev.wbCycle + 1);
                    inst.exCycle = inst.idCycle + 1;
                    inst.memCycle = inst.exCycle + 1;
                    inst.wbCycle = inst.memCycle + 1;
                }
            }

            // Ensure each instruction's own stages are properly spaced
            inst.exCycle = std::max(inst.exCycle, inst.idCycle + 1);
            inst.memCycle = std::max(inst.memCycle, inst.exCycle + 1);
            inst.wbCycle = std::max(inst.wbCycle, inst.memCycle + 1);
        }

        return instructions.back().wbCycle;
    }
};

int main() {
    int N;
    std::cin >> N;
    std::cin.ignore();  // Clear newline

    PipelineSimulator simulator;
    std::string line;

    for (int i = 0; i < N; i++) {
        std::getline(std::cin, line);
        simulator.addInstruction(line);
    }

    std::cout << simulator.simulate() << std::endl;
    return 0;
}
