#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>

struct Instruction {
    enum Type { LOAD, STORE, ADD, SUB } type;
    std::string destReg;
    std::string src1Reg;
    std::string src2Reg;
    std::string memLoc;
    bool isImmediate1 = false;
    bool isImmediate2 = false;
    int ifCycle = 0;
    int idCycle = 0;
    int exCycle = 0;
    int memCycle = 0;
    int wbCycle = 0;
    bool stalled = false;

    std::string toString() const {
        switch(type) {
            case LOAD: return "LOAD";
            case STORE: return "STORE";
            case ADD: return "ADD";
            case SUB: return "SUB";
            default: return "UNKNOWN";
        }
    }
};

class PipelineSimulator {
private:
    std::vector<Instruction> instructions;

    void debugPrint(const std::string& msg) {
        std::cerr << msg << std::endl;
    }

    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }

    bool parseLoadStore(const std::string& line, Instruction& inst) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) return false;

        size_t typeEnd = trimmed.find(' ');
        if (typeEnd == std::string::npos) return false;

        std::string type = trimmed.substr(0, typeEnd);
        if (type != "LOAD" && type != "STORE") return false;

        size_t commaPos = trimmed.find(',', typeEnd);
        if (commaPos == std::string::npos) return false;

        std::string reg = trim(trimmed.substr(typeEnd, commaPos - typeEnd));
        std::string mem = trim(trimmed.substr(commaPos + 1));

        if (type == "LOAD") {
            inst.type = Instruction::LOAD;
            inst.destReg = reg;
            inst.memLoc = mem;
            debugPrint("Parsed LOAD: dest=" + inst.destReg + " mem=" + inst.memLoc);
        } else {
            inst.type = Instruction::STORE;
            inst.src1Reg = reg;
            inst.memLoc = mem;
            debugPrint("Parsed STORE: src=" + inst.src1Reg + " mem=" + inst.memLoc);
        }
        return true;
    }

    bool parseAddSub(const std::string& line, Instruction& inst) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) return false;

        std::vector<std::string> tokens;
        std::istringstream iss(trimmed);
        std::string token;

        while (iss >> token) {
            if (token.back() == ',') token.pop_back();
            tokens.push_back(token);
        }

        if (tokens.size() < 4) return false;
        if (tokens[0] != "ADD" && tokens[0] != "SUB") return false;

        inst.type = (tokens[0] == "ADD") ? Instruction::ADD : Instruction::SUB;
        inst.destReg = tokens[1];

        if (tokens[2][0] == 'R') {
            inst.src1Reg = tokens[2];
            inst.isImmediate1 = false;
        } else {
            inst.isImmediate1 = true;
        }

        if (tokens[3][0] == 'R') {
            inst.src2Reg = tokens[3];
            inst.isImmediate2 = false;
        } else {
            inst.isImmediate2 = true;
        }


        debugPrint("Parsed " + tokens[0] + ": dest=" + inst.destReg +
                  " src1=" + (inst.isImmediate1 ? "imm" : inst.src1Reg) +
                  " src2=" + (inst.isImmediate2 ? "imm" : inst.src2Reg));
        return true;
    }

public:
    void addInstruction(const std::string& line) {
        debugPrint("\nParsing instruction: " + line);
        Instruction inst;
        std::string trimmed = trim(line);
        if (trimmed.empty()) return;

        if (!parseLoadStore(trimmed, inst) && !parseAddSub(trimmed, inst)) {
            debugPrint("Error: Unknown instruction type: " + trimmed);
            return;
        }

        instructions.push_back(inst);
    }

    bool checkDependencies(size_t currIdx, int cycle) {
        const auto& curr = instructions[currIdx];

        for (size_t i = 0; i < currIdx; i++) {
            const auto& prev = instructions[i];

            // Determine when data is available
            int dataAvailableCycle;
            if (prev.type == Instruction::LOAD) {
                dataAvailableCycle = prev.wbCycle;  // Data available after WB for LOAD
            } else if (prev.type == Instruction::ADD || prev.type == Instruction::SUB) {
                dataAvailableCycle = prev.memCycle;  // Data available after MEM for ADD/SUB
            } else {
                dataAvailableCycle = prev.memCycle;  // For STORE
            }

            // Check RAW dependencies
            if (!prev.destReg.empty()) {
                if ((!curr.isImmediate1 && curr.src1Reg == prev.destReg) ||
                    (!curr.isImmediate2 && curr.src2Reg == prev.destReg)) {
                    if (cycle <= dataAvailableCycle) {
                        debugPrint("RAW hazard: Instruction " + std::to_string(currIdx) +
                                 " waiting for data from instruction " + std::to_string(i));
                        return false;
                    }
                }
            }

            // Check WAR dependencies
            if (!curr.destReg.empty()) {
                if ((!prev.isImmediate1 && curr.destReg == prev.src1Reg) ||
                    (!prev.isImmediate2 && curr.destReg == prev.src2Reg)) {
                    if (cycle <= prev.idCycle) {
                        debugPrint("WAR hazard: Instruction " + std::to_string(currIdx) +
                                 " waiting for register read in instruction " + std::to_string(i));
                        return false;
                    }
                }
            }

            // Check WAW dependencies
            if (!curr.destReg.empty() && curr.destReg == prev.destReg) {
                if (cycle <= prev.wbCycle) {
                    debugPrint("WAW hazard: Instruction " + std::to_string(currIdx) +
                             " waiting for write completion in instruction " + std::to_string(i));
                    return false;
                }
            }

            // Check memory dependencies
            if ((curr.type == Instruction::LOAD || curr.type == Instruction::STORE) &&
                (prev.type == Instruction::LOAD || prev.type == Instruction::STORE)) {
                if (curr.memLoc == prev.memLoc && cycle <= prev.memCycle) {
                    debugPrint("Memory hazard: Instruction " + std::to_string(currIdx) +
                             " waiting for memory access in instruction " + std::to_string(i));
                    return false;
                }
            }

            // Check structural hazards for memory unit
            if ((curr.type == Instruction::LOAD || curr.type == Instruction::STORE) &&
                (prev.type == Instruction::LOAD || prev.type == Instruction::STORE)) {
                if (cycle + 2 == prev.memCycle) {  // Only check exact overlap
                    debugPrint("Structural hazard: Memory unit busy");
                    return false;
                }
            }
        }

        return true;
    }

    int simulate() {
        if (instructions.empty()) return 0;
        debugPrint("\nStarting simulation with " + std::to_string(instructions.size()) + " instructions");

        // First instruction
        auto& first = instructions[0];
        first.ifCycle = 1;
        first.idCycle = 2;
        first.exCycle = 3;
        first.memCycle = 4;
        first.wbCycle = first.type == Instruction::LOAD ? 6 : 5;

        debugPrintStages(first, 0);

        // Process remaining instructions
        for (size_t i = 1; i < instructions.size(); i++) {
            auto& curr = instructions[i];
            debugPrint("\nProcessing instruction " + std::to_string(i) + ":");

            // Start with basic timing
            curr.ifCycle = instructions[i-1].ifCycle + 1;
            curr.idCycle = curr.ifCycle + 1;

            // Check for dependencies and stall if necessary
            while (!checkDependencies(i, curr.idCycle)) {
                curr.ifCycle++;
                curr.idCycle++;
                curr.stalled = true;
            }

            // Set remaining stages
            curr.exCycle = curr.idCycle + 1;
            curr.memCycle = curr.exCycle + 1;
            curr.wbCycle = curr.memCycle + (curr.type == Instruction::LOAD ? 2 : 1);

            // Additional stall for memory operations
            if ((curr.type == Instruction::LOAD || curr.type == Instruction::STORE) &&
                i > 0 && (instructions[i-1].type == Instruction::LOAD || instructions[i-1].type == Instruction::STORE)) {
                int stallCycles = 1;  // Reduced stall cycles
                curr.ifCycle += stallCycles;
                curr.idCycle += stallCycles;
                curr.exCycle += stallCycles;
                curr.memCycle += stallCycles;
                curr.wbCycle += stallCycles;
            }

            // Additional stall for dependent ALU operations
            if ((curr.type == Instruction::ADD || curr.type == Instruction::SUB) &&
                i > 0 && (instructions[i-1].type == Instruction::ADD || instructions[i-1].type == Instruction::SUB)) {
                bool hasRegDependency = false;
                if (!curr.isImmediate1 && (curr.src1Reg == instructions[i-1].destReg)) {
                    hasRegDependency = true;
                }
                if (!curr.isImmediate2 && (curr.src2Reg == instructions[i-1].destReg)) {
                    hasRegDependency = true;
                }
                if (hasRegDependency) {
                    int stallCycles = 2;  // Increased stall cycles for dependent ALU ops
                    curr.ifCycle += stallCycles;
                    curr.idCycle += stallCycles;
                    curr.exCycle += stallCycles;
                    curr.memCycle += stallCycles;
                    curr.wbCycle += stallCycles;
                }
            }

            debugPrintStages(curr, i);
        }

        int maxCycle = 0;
        for (const auto& inst : instructions) {
            maxCycle = std::max(maxCycle, inst.wbCycle);
        }

        debugPrint("\nSimulation complete. Total cycles: " + std::to_string(maxCycle));
        return maxCycle;
    }

private:
    void debugPrintStages(const Instruction& inst, int idx) {
        std::cerr << "Instruction " << idx << " (" << inst.toString() << "): ";
        if (!inst.destReg.empty()) std::cerr << "dest=" << inst.destReg << " ";
        if (!inst.src1Reg.empty()) std::cerr << "src1=" << inst.src1Reg << " ";
        if (!inst.src2Reg.empty()) std::cerr << "src2=" << inst.src2Reg << " ";
        if (!inst.memLoc.empty()) std::cerr << "mem=" << inst.memLoc << " ";
        std::cerr << "IF=" << inst.ifCycle << " ID=" << inst.idCycle
                 << " EX=" << inst.exCycle << " MEM=" << inst.memCycle
                 << " WB=" << inst.wbCycle;
        if (inst.stalled) std::cerr << " (STALLED)";
        std::cerr << std::endl;
    }
};

int main() {
    int n;
    std::string line;

    if (!(std::cin >> n)) {
        std::cerr << "Error: Failed to read number of instructions" << std::endl;
        return 1;
    }

    std::getline(std::cin, line);  // Clear newline

    PipelineSimulator simulator;
    for (int i = 0; i < n; i++) {
        if (!std::getline(std::cin, line)) {
            std::cerr << "Error: Failed to read instruction " << i + 1 << std::endl;
            return 1;
        }
        simulator.addInstruction(line);
    }

    std::cout << simulator.simulate() << std::endl;
    return 0;
}
