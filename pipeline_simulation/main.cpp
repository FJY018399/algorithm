#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <set>

struct Instruction {
    enum Type { LOAD, STORE, ADD, SUB };
    Type type;
    std::string destReg;
    std::string src1Reg;  // For ADD/SUB first source register
    std::string src2Reg;  // For ADD/SUB second source register
    std::string memLoc;   // For LOAD/STORE memory location
    int ifCycle = 1;      // Start with IF at cycle 1
    int idCycle = 2;      // ID follows IF
    int exCycle = 3;      // EX follows ID
    int memCycle = 4;     // MEM follows EX
    int wbCycle = 5;      // WB follows MEM
};

class PipelineSimulator {
private:
    std::vector<Instruction> instructions;

    bool isRegister(const std::string& op) {
        return op[0] == 'R';
    }

    // Check if inst2 depends on inst1
    bool hasDependency(const Instruction& inst1, const Instruction& inst2) {
        // Check write-after-write (WAW)
        if (!inst2.destReg.empty() && inst2.destReg == inst1.destReg) {
            return true;
        }

        // Check read-after-write (RAW)
        if (inst2.type == Instruction::ADD || inst2.type == Instruction::SUB) {
            if (inst1.destReg == inst2.src1Reg || inst1.destReg == inst2.src2Reg) {
                return true;
            }
        } else if (inst2.type == Instruction::STORE && inst1.destReg == inst2.src1Reg) {
            return true;
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
            std::string reg, mem;
            ss >> reg >> mem;
            inst.destReg = reg.substr(0, reg.length() - 1);  // Remove comma
            inst.memLoc = mem;
        } else if (type == "STORE") {
            inst.type = Instruction::STORE;
            std::string reg, mem;
            ss >> reg >> mem;
            inst.src1Reg = reg.substr(0, reg.length() - 1);  // Remove comma, store reads from register
            inst.memLoc = mem;
        } else if (type == "ADD" || type == "SUB") {
            inst.type = (type == "ADD") ? Instruction::ADD : Instruction::SUB;
            std::string dest, src1, src2;
            ss >> dest >> src1 >> src2;

            // Clean up register names
            inst.destReg = dest.substr(0, dest.length() - 1);  // Remove comma
            inst.src1Reg = (src1[0] == 'R') ? src1.substr(0, src1.length() - 1) : "";  // Remove comma if register
            inst.src2Reg = (src2[0] == 'R') ? src2 : "";  // Last operand has no comma
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

            // Each instruction starts immediately after the previous one's IF stage
            inst.ifCycle = i + 1;  // Instructions start in consecutive cycles
            inst.idCycle = inst.ifCycle + 1;
            inst.exCycle = inst.idCycle + 1;
            inst.memCycle = inst.exCycle + 1;
            inst.wbCycle = inst.memCycle + 1;

            // Check dependencies with all previous instructions
            for (size_t j = 0; j < i; j++) {
                const Instruction& prev = instructions[j];

                if (hasDependency(prev, inst)) {
                    // For data dependencies, only need to wait for the stage where data becomes available
                    int requiredCycle;

                    if (prev.type == Instruction::LOAD) {
                        // For LOAD, data is available after MEM stage
                        requiredCycle = prev.memCycle;
                    } else {
                        // For arithmetic, data is available after EX stage
                        requiredCycle = prev.exCycle;
                    }

                    // Only stall ID stage if it would start before data is ready
                    if (inst.idCycle <= requiredCycle) {
                        // Adjust ID stage to start right after data is ready
                        inst.idCycle = requiredCycle + 1;
                        inst.exCycle = inst.idCycle + 1;
                        inst.memCycle = inst.exCycle + 1;
                        inst.wbCycle = inst.memCycle + 1;
                    }
                }

                // Memory operations must complete in order
                if ((inst.type == Instruction::LOAD || inst.type == Instruction::STORE) &&
                    (prev.type == Instruction::LOAD || prev.type == Instruction::STORE) &&
                    j == i - 1) {  // Only for consecutive memory operations
                    if (inst.memCycle <= prev.memCycle) {
                        // Memory operations must be sequential
                        inst.memCycle = prev.memCycle + 1;
                        inst.wbCycle = inst.memCycle + 1;
                    }
                }
            }
        }

        // Return the cycle when the last instruction completes
        return instructions.back().wbCycle;
    }

int main() {
    int n;
    std::cin >> n;

    std::vector<std::string> instructions(n);
    for(int i = 0; i < n; i++) {
        std::string line;
        std::getline(std::cin >> std::ws, line);
        instructions[i] = line;
    }

    PipelineSimulator simulator;
    for(const auto& inst : instructions) {
        simulator.addInstruction(inst);
    }

    std::cout << simulator.simulate() << std::endl;
    return 0;
}
