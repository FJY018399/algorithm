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
        // LOAD instruction dependencies
        if (inst1.type == Instruction::LOAD) {
            // If inst1 is LOAD, inst2 can't use the loaded register until LOAD completes
            if (inst2.type == Instruction::ADD || inst2.type == Instruction::SUB) {
                // Check if inst2 uses the register being loaded
                return inst2.destReg == inst1.destReg ||
                       (isRegister(inst2.op1) && inst2.op1 == inst1.destReg) ||
                       (isRegister(inst2.op2) && inst2.op2 == inst1.destReg);
            } else if (inst2.type == Instruction::STORE) {
                return inst2.destReg == inst1.destReg;
            }
        }
        // STORE instruction dependencies
        else if (inst1.type == Instruction::STORE) {
            // For STORE, block if inst2 tries to modify the source register
            if (inst2.type == Instruction::ADD || inst2.type == Instruction::SUB) {
                return inst2.destReg == inst1.destReg;
            } else if (inst2.type == Instruction::STORE || inst2.type == Instruction::LOAD) {
                // Ensure sequential memory operations
                return true;
            }
        }
        // ADD/SUB instruction dependencies
        else if (inst1.type == Instruction::ADD || inst1.type == Instruction::SUB) {
            if (inst2.type == Instruction::ADD || inst2.type == Instruction::SUB) {
                return inst2.destReg == inst1.destReg ||
                       (isRegister(inst2.op1) && inst2.op1 == inst1.destReg) ||
                       (isRegister(inst2.op2) && inst2.op2 == inst1.destReg);
            } else if (inst2.type == Instruction::STORE) {
                return inst2.destReg == inst1.destReg;
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

    void printStages(const Instruction& inst, int i) {
        std::string type;
        switch(inst.type) {
            case Instruction::LOAD: type = "LOAD"; break;
            case Instruction::STORE: type = "STORE"; break;
            case Instruction::ADD: type = "ADD"; break;
            case Instruction::SUB: type = "SUB"; break;
        }
        std::cout << "Instruction " << i << " (" << type << " " << inst.destReg;
        if (inst.type == Instruction::ADD || inst.type == Instruction::SUB) {
            std::cout << ", " << inst.op1 << ", " << inst.op2;
        } else {
            std::cout << ", " << inst.op1;
        }
        std::cout << "):" << std::endl;
        std::cout << "  IF: " << inst.ifCycle << std::endl;
        std::cout << "  ID: " << inst.idCycle << std::endl;
        std::cout << "  EX: " << inst.exCycle << std::endl;
        std::cout << "  MEM: " << inst.memCycle << std::endl;
        std::cout << "  WB: " << inst.wbCycle << std::endl;
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

            // Start IF one cycle after previous instruction's IF (in-order issue)
            inst.ifCycle = prev.ifCycle + 1;  // Base delay between instructions

            // Initially set remaining stages based on IF cycle
            inst.idCycle = inst.ifCycle + 1;  // ID must follow IF
            inst.exCycle = inst.idCycle + 1;  // EX must follow ID
            inst.memCycle = inst.exCycle + 1;  // MEM must follow EX
            inst.wbCycle = inst.memCycle + 1;  // WB must follow MEM

            // Find dependencies and their types
            bool hasRegDep = false;
            bool hasLoadStoreDep = false;
            int maxDepCycle = inst.idCycle;

            // Check dependencies with ALL previous instructions
            for (size_t j = 0; j < i; j++) {
                const Instruction& dep = instructions[j];
                if (hasDependency(dep, inst)) {
                    hasRegDep = true;
                    // Must wait for previous instruction to complete WB stage
                    maxDepCycle = std::max(maxDepCycle, dep.wbCycle);
                    if (dep.type == Instruction::LOAD || dep.type == Instruction::STORE) {
                        hasLoadStoreDep = true;
                    }
                }
            }

            // Always ensure minimum pipeline spacing, even for independent instructions
            inst.idCycle = std::max(inst.idCycle, prev.idCycle + 1);
            inst.exCycle = std::max(inst.exCycle, prev.exCycle + 1);
            inst.memCycle = std::max(inst.memCycle, prev.memCycle + 1);
            inst.wbCycle = std::max(inst.wbCycle, prev.wbCycle + 1);

            // Handle register dependencies (ADD/SUB)
            if (hasRegDep && !hasLoadStoreDep) {
                // For pure register dependencies, wait for WB but minimize delay
                inst.idCycle = std::max(maxDepCycle, inst.idCycle);
                inst.exCycle = inst.idCycle + 1;
                inst.memCycle = inst.exCycle + 1;
                inst.wbCycle = inst.memCycle + 1;
            }

            // Special handling for LOAD/STORE instructions and their dependencies
            if (inst.type == Instruction::LOAD || inst.type == Instruction::STORE || hasLoadStoreDep) {
                if (hasLoadStoreDep) {
                    // Must wait longer for LOAD/STORE dependencies
                    inst.idCycle = std::max(maxDepCycle + 1, inst.idCycle);
                    inst.exCycle = inst.idCycle + 1;
                    inst.memCycle = inst.exCycle + 1;
                    inst.wbCycle = inst.memCycle + 1;
                }

                if (inst.type == Instruction::LOAD || inst.type == Instruction::STORE) {
                    // Ensure memory operations are properly spaced
                    inst.memCycle = std::max(inst.memCycle, prev.memCycle + 2);
                    inst.wbCycle = inst.memCycle + 1;
                }
            }

            // Ensure each stage follows its previous stage by exactly one cycle
            inst.exCycle = std::max(inst.exCycle, inst.idCycle + 1);
            inst.memCycle = std::max(inst.memCycle, inst.exCycle + 1);
            inst.wbCycle = std::max(inst.wbCycle, inst.memCycle + 1);

            // Debug output
            std::cout << "\nProcessing instruction " << i << ":" << std::endl;
            printStages(inst, i);
            if (hasRegDep) std::cout << "  Has register dependency" << std::endl;
            if (hasLoadStoreDep) std::cout << "  Has LOAD/STORE dependency" << std::endl;
        } // Close for loop


        return instructions.back().wbCycle;
    } // Close simulate function
}; // Close class

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
