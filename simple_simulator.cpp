#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <map>

using namespace std;

// Simple RISC-V simulator - non-pipelined for correctness
class SimpleRISCVSimulator {
private:
    static const int NUM_REGS = 32;
    static const uint32_t PC_START = 0x00000000;
    
    uint32_t regs[NUM_REGS];
    uint32_t pc;
    vector<uint8_t> memory;
    const size_t MEMORY_SIZE = 0x100000; // 1MB
    
public:
    SimpleRISCVSimulator() : pc(PC_START) {
        memset(regs, 0, sizeof(regs));
        memory.resize(MEMORY_SIZE, 0);
    }
    
    void load_program(const vector<uint8_t>& program) {
        if (program.size() > MEMORY_SIZE) {
            throw runtime_error("Program too large for memory");
        }
        
        for (size_t i = 0; i < program.size(); i++) {
            memory[i] = program[i];
        }
    }
    
    uint32_t fetch_instruction(uint32_t address) {
        if (address + 3 >= MEMORY_SIZE) {
            throw runtime_error("Instruction fetch out of bounds");
        }
        
        return memory[address] | (memory[address + 1] << 8) | 
               (memory[address + 2] << 16) | (memory[address + 3] << 24);
    }
    
    void execute_instruction(uint32_t instr) {
        uint32_t opcode = instr & 0x7F;
        uint32_t rd = (instr >> 7) & 0x1F;
        uint32_t funct3 = (instr >> 12) & 0x7;
        uint32_t rs1 = (instr >> 15) & 0x1F;
        uint32_t rs2 = (instr >> 20) & 0x1F;
        int32_t imm = static_cast<int32_t>(instr) >> 20;
        
        switch (opcode) {
            case 0b0010011: // I-type arithmetic
                switch (funct3) {
                    case 0b000: // addi
                        if (rd != 0) regs[rd] = regs[rs1] + imm;
                        break;
                    case 0b001: // slli
                        if (rd != 0) regs[rd] = regs[rs1] << (imm & 0x1F);
                        break;
                    case 0b010: // slti
                        if (rd != 0) regs[rd] = (static_cast<int32_t>(regs[rs1]) < imm) ? 1 : 0;
                        break;
                    case 0b011: // sltiu
                        if (rd != 0) regs[rd] = (regs[rs1] < static_cast<uint32_t>(imm)) ? 1 : 0;
                        break;
                    case 0b100: // xori
                        if (rd != 0) regs[rd] = regs[rs1] ^ imm;
                        break;
                    case 0b101: // srli/srai
                        if ((imm >> 5) & 0x20) { // srai
                            if (rd != 0) regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(regs[rs1]) >> (imm & 0x1F));
                        } else { // srli
                            if (rd != 0) regs[rd] = regs[rs1] >> (imm & 0x1F);
                        }
                        break;
                    case 0b110: // ori
                        if (rd != 0) regs[rd] = regs[rs1] | imm;
                        break;
                    case 0b111: // andi
                        if (rd != 0) regs[rd] = regs[rs1] & imm;
                        break;
                }
                break;
                
            case 0b0110011: // R-type
                {
                    uint32_t funct7 = (instr >> 25) & 0x7F;
                    switch (funct3) {
                        case 0b000: // add/sub
                            if (funct7 == 0b0100000) { // sub
                                if (rd != 0) regs[rd] = regs[rs1] - regs[rs2];
                            } else { // add
                                if (rd != 0) regs[rd] = regs[rs1] + regs[rs2];
                            }
                            break;
                        case 0b001: // sll
                            if (rd != 0) regs[rd] = regs[rs1] << (regs[rs2] & 0x1F);
                            break;
                        case 0b010: // slt
                            if (rd != 0) regs[rd] = (static_cast<int32_t>(regs[rs1]) < static_cast<int32_t>(regs[rs2])) ? 1 : 0;
                            break;
                        case 0b011: // sltu
                            if (rd != 0) regs[rd] = (regs[rs1] < regs[rs2]) ? 1 : 0;
                            break;
                        case 0b100: // xor
                            if (rd != 0) regs[rd] = regs[rs1] ^ regs[rs2];
                            break;
                        case 0b101: // srl/sra
                            if (funct7 == 0b0100000) { // sra
                                if (rd != 0) regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(regs[rs1]) >> (regs[rs2] & 0x1F));
                            } else { // srl
                                if (rd != 0) regs[rd] = regs[rs1] >> (regs[rs2] & 0x1F);
                            }
                            break;
                        case 0b110: // or
                            if (rd != 0) regs[rd] = regs[rs1] | regs[rs2];
                            break;
                        case 0b111: // and
                            if (rd != 0) regs[rd] = regs[rs1] & regs[rs2];
                            break;
                    }
                }
                break;
                
            case 0b0000011: // Load
                {
                    uint32_t addr = regs[rs1] + imm;
                    switch (funct3) {
                        case 0b000: // lb
                            if (addr < MEMORY_SIZE && rd != 0) {
                                int8_t val = static_cast<int8_t>(memory[addr]);
                                regs[rd] = static_cast<uint32_t>(val);
                            }
                            break;
                        case 0b001: // lh
                            if (addr + 1 < MEMORY_SIZE && rd != 0) {
                                int16_t val = static_cast<int16_t>(memory[addr] | (memory[addr + 1] << 8));
                                regs[rd] = static_cast<uint32_t>(val);
                            }
                            break;
                        case 0b010: // lw
                            if (addr + 3 < MEMORY_SIZE && rd != 0) {
                                uint32_t val = memory[addr] | (memory[addr + 1] << 8) | 
                                              (memory[addr + 2] << 16) | (memory[addr + 3] << 24);
                                regs[rd] = val;
                            }
                            break;
                        case 0b100: // lbu
                            if (addr < MEMORY_SIZE && rd != 0) {
                                regs[rd] = memory[addr];
                            }
                            break;
                        case 0b101: // lhu
                            if (addr + 1 < MEMORY_SIZE && rd != 0) {
                                regs[rd] = memory[addr] | (memory[addr + 1] << 8);
                            }
                            break;
                    }
                }
                break;
                
            case 0b0100011: // Store
                {
                    uint32_t addr = regs[rs1] + imm;
                    uint32_t data = regs[rs2];
                    switch (funct3) {
                        case 0b000: // sb
                            if (addr < MEMORY_SIZE) {
                                memory[addr] = data & 0xFF;
                            }
                            break;
                        case 0b001: // sh
                            if (addr + 1 < MEMORY_SIZE) {
                                memory[addr] = data & 0xFF;
                                memory[addr + 1] = (data >> 8) & 0xFF;
                            }
                            break;
                        case 0b010: // sw
                            if (addr + 3 < MEMORY_SIZE) {
                                memory[addr] = data & 0xFF;
                                memory[addr + 1] = (data >> 8) & 0xFF;
                                memory[addr + 2] = (data >> 16) & 0xFF;
                                memory[addr + 3] = (data >> 24) & 0xFF;
                            }
                            break;
                    }
                }
                break;
                
            case 0b1100011: // Branch
                {
                    bool taken = false;
                    switch (funct3) {
                        case 0b000: // beq
                            taken = (regs[rs1] == regs[rs2]);
                            break;
                        case 0b001: // bne
                            taken = (regs[rs1] != regs[rs2]);
                            break;
                        case 0b100: // blt
                            taken = (static_cast<int32_t>(regs[rs1]) < static_cast<int32_t>(regs[rs2]));
                            break;
                        case 0b101: // bge
                            taken = (static_cast<int32_t>(regs[rs1]) >= static_cast<int32_t>(regs[rs2]));
                            break;
                        case 0b110: // bltu
                            taken = (regs[rs1] < regs[rs2]);
                            break;
                        case 0b111: // bgeu
                            taken = (regs[rs1] >= regs[rs2]);
                            break;
                    }
                    
                    if (taken) {
                        // Sign extend and shift branch immediate
                        int32_t branch_imm = ((instr >> 31) & 0x1) << 12 |
                                           ((instr >> 7) & 0x1) << 11 |
                                           ((instr >> 25) & 0x3F) << 5 |
                                           ((instr >> 8) & 0xF) << 1;
                        if (branch_imm & 0x1000) branch_imm |= 0xFFFFE000;
                        pc = pc + branch_imm - 4; // -4 because we'll add 4 later
                    }
                }
                break;
                
            case 0b0110111: // lui
                if (rd != 0) {
                    regs[rd] = instr & 0xFFFFF000;
                }
                break;
                
            case 0b0010111: // auipc
                if (rd != 0) {
                    regs[rd] = pc + (instr & 0xFFFFF000);
                }
                break;
                
            case 0b1101111: // jal
                if (rd != 0) {
                    regs[rd] = pc + 4; // Return address
                }
                {
                    int32_t jump_imm = ((instr >> 31) & 0x1) << 20 |
                                      ((instr >> 12) & 0xFF) << 12 |
                                      ((instr >> 20) & 0x1) << 11 |
                                      ((instr >> 21) & 0x3FF) << 1;
                    if (jump_imm & 0x100000) jump_imm |= 0xFFE00000;
                    pc = pc + jump_imm - 4; // -4 because we'll add 4 later
                }
                break;
                
            case 0b1100111: // jalr
                if (rd != 0) {
                    regs[rd] = pc + 4; // Return address
                }
                pc = (regs[rs1] + imm) & ~1; // Clear LSB
                pc -= 4; // -4 because we'll add 4 later
                break;
        }
    }
    
    void run() {
        uint32_t max_instructions = 1000000; // Safety limit
        uint32_t instruction_count = 0;
        
        while (instruction_count < max_instructions) {
            try {
                uint32_t instr = fetch_instruction(pc);
                execute_instruction(instr);
                pc += 4;
                instruction_count++;
            } catch (const runtime_error& e) {
                // End of program
                break;
            }
        }
    }
    
    uint32_t get_return_value() const {
        return regs[10]; // Return value in a0 (x10)
    }
};

// Read binary data from stdin
vector<uint8_t> read_input_data() {
    vector<uint8_t> data;
    char byte;
    
    while (cin.get(byte)) {
        data.push_back(static_cast<uint8_t>(byte));
    }
    
    return data;
}

int main() {
    try {
        vector<uint8_t> program_data = read_input_data();
        
        if (program_data.empty()) {
            cerr << "Error: No input data provided" << endl;
            return 1;
        }
        
        SimpleRISCVSimulator simulator;
        simulator.load_program(program_data);
        simulator.run();
        
        uint32_t result = simulator.get_return_value();
        cout << result << endl;
        
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}