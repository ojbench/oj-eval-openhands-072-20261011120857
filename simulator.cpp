#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <map>
#include <algorithm>

using namespace std;

// RISC-V Register indices
const int NUM_REGS = 32;
const uint32_t PC_START = 0x00000000;

// Instruction types and opcodes
enum class InstrType {
    R_TYPE, I_TYPE, S_TYPE, B_TYPE, U_TYPE, J_TYPE, UNKNOWN
};

// Pipeline stages
enum class PipelineStage {
    IF, ID, EX, MEM, WB
};

// Instruction structure
struct Instruction {
    uint32_t raw;
    InstrType type;
    uint32_t opcode;
    uint32_t rd, rs1, rs2;
    uint32_t funct3, funct7;
    int32_t imm;
    string name;
    
    Instruction() : raw(0), type(InstrType::UNKNOWN), opcode(0), 
                   rd(0), rs1(0), rs2(0), funct3(0), funct7(0), imm(0), name("") {}
};

// Pipeline register structure
struct PipelineReg {
    Instruction instr;
    uint32_t pc;
    uint32_t val1, val2;
    uint32_t alu_result;
    uint32_t mem_data;
    bool valid;
    
    PipelineReg() : pc(0), val1(0), val2(0), alu_result(0), mem_data(0), valid(false) {}
};

class RISCVSimulator {
private:
    // Registers
    uint32_t regs[NUM_REGS];
    uint32_t pc;
    
    // Memory
    vector<uint8_t> memory;
    const size_t MEMORY_SIZE = 0x100000; // 1MB
    
    // Pipeline registers
    PipelineReg if_id, id_ex, ex_mem, mem_wb;
    
    // Control signals
    bool stall, flush;
    
    // Statistics
    uint64_t cycle_count;
    uint64_t instruction_count;
    
    // Instruction decode map
    map<uint32_t, string> opcode_names;
    map<uint32_t, map<uint32_t, string>> funct3_names;
    
public:
    RISCVSimulator() : pc(PC_START), stall(false), flush(false), 
                       cycle_count(0), instruction_count(0) {
        // Initialize registers
        memset(regs, 0, sizeof(regs));
        
        // Initialize memory
        memory.resize(MEMORY_SIZE, 0);
        
        // Initialize instruction names
        init_instruction_names();
    }
    
    void init_instruction_names() {
        // R-type instructions
        opcode_names[0b0110011] = "R_TYPE";
        funct3_names[0b0110011][0b000] = "add/sub";
        funct3_names[0b0110011][0b001] = "sll";
        funct3_names[0b0110011][0b010] = "slt";
        funct3_names[0b0110011][0b011] = "sltu";
        funct3_names[0b0110011][0b100] = "xor";
        funct3_names[0b0110011][0b101] = "srl/sra";
        funct3_names[0b0110011][0b110] = "or";
        funct3_names[0b0110011][0b111] = "and";
        
        // I-type instructions
        opcode_names[0b0010011] = "I_TYPE_ARITH";
        opcode_names[0b0000011] = "LOAD";
        opcode_names[0b1100111] = "JALR";
        
        // S-type instructions
        opcode_names[0b0100011] = "STORE";
        
        // B-type instructions
        opcode_names[0b1100011] = "BRANCH";
        
        // U-type instructions
        opcode_names[0b0110111] = "LUI";
        opcode_names[0b0010111] = "AUIPC";
        
        // J-type instructions
        opcode_names[0b1101111] = "JAL";
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
        
        uint32_t instr = 0;
        instr |= (static_cast<uint32_t>(memory[address]) << 0);
        instr |= (static_cast<uint32_t>(memory[address + 1]) << 8);
        instr |= (static_cast<uint32_t>(memory[address + 2]) << 16);
        instr |= (static_cast<uint32_t>(memory[address + 3]) << 24);
        
        return instr;
    }
    
    Instruction decode_instruction(uint32_t raw_instr) {
        Instruction instr;
        instr.raw = raw_instr;
        instr.opcode = raw_instr & 0x7F;
        
        // Determine instruction type based on opcode
        switch (instr.opcode) {
            case 0b0110011: // R-type
                instr.type = InstrType::R_TYPE;
                instr.rd = (raw_instr >> 7) & 0x1F;
                instr.funct3 = (raw_instr >> 12) & 0x7;
                instr.rs1 = (raw_instr >> 15) & 0x1F;
                instr.rs2 = (raw_instr >> 20) & 0x1F;
                instr.funct7 = (raw_instr >> 25) & 0x7F;
                break;
                
            case 0b0010011: // I-type arithmetic
            case 0b0000011: // I-type load
            case 0b1100111: // I-type jalr
                instr.type = InstrType::I_TYPE;
                instr.rd = (raw_instr >> 7) & 0x1F;
                instr.funct3 = (raw_instr >> 12) & 0x7;
                instr.rs1 = (raw_instr >> 15) & 0x1F;
                instr.imm = (raw_instr >> 20) & 0xFFF;
                // Sign extend immediate
                if (instr.imm & 0x800) {
                    instr.imm |= 0xFFFFF000;
                }
                break;
                
            case 0b0100011: // S-type
                instr.type = InstrType::S_TYPE;
                instr.imm = (raw_instr >> 7) & 0x1F;
                instr.funct3 = (raw_instr >> 12) & 0x7;
                instr.rs1 = (raw_instr >> 15) & 0x1F;
                instr.rs2 = (raw_instr >> 20) & 0x1F;
                instr.imm |= ((raw_instr >> 25) & 0x7F) << 5;
                // Sign extend immediate
                if (instr.imm & 0x800) {
                    instr.imm |= 0xFFFFF000;
                }
                break;
                
            case 0b1100011: // B-type
                instr.type = InstrType::B_TYPE;
                instr.imm = (raw_instr >> 8) & 0xF;
                instr.funct3 = (raw_instr >> 12) & 0x7;
                instr.rs1 = (raw_instr >> 15) & 0x1F;
                instr.rs2 = (raw_instr >> 20) & 0x1F;
                instr.imm |= ((raw_instr >> 7) & 0x1) << 4;
                instr.imm |= ((raw_instr >> 25) & 0x3F) << 5;
                instr.imm |= ((raw_instr >> 31) & 0x1) << 11;
                // Sign extend immediate
                if (instr.imm & 0x1000) {
                    instr.imm |= 0xFFFFE000;
                }
                instr.imm <<= 1; // Branch offset is multiplied by 2
                break;
                
            case 0b0110111: // U-type lui
            case 0b0010111: // U-type auipc
                instr.type = InstrType::U_TYPE;
                instr.rd = (raw_instr >> 7) & 0x1F;
                instr.imm = raw_instr & 0xFFFFF000;
                break;
                
            case 0b1101111: // J-type
                instr.type = InstrType::J_TYPE;
                instr.rd = (raw_instr >> 7) & 0x1F;
                instr.imm = (raw_instr >> 21) & 0x3FF;
                instr.imm |= ((raw_instr >> 20) & 0x1) << 10;
                instr.imm |= ((raw_instr >> 12) & 0xFF) << 11;
                instr.imm |= ((raw_instr >> 31) & 0x1) << 19;
                // Sign extend immediate
                if (instr.imm & 0x100000) {
                    instr.imm |= 0xFFE00000;
                }
                instr.imm <<= 1; // Jump offset is multiplied by 2
                break;
                
            default:
                instr.type = InstrType::UNKNOWN;
                break;
        }
        
        // Set instruction name for debugging
        if (opcode_names.find(instr.opcode) != opcode_names.end()) {
            instr.name = opcode_names[instr.opcode];
            if (funct3_names.find(instr.opcode) != funct3_names.end() &&
                funct3_names[instr.opcode].find(instr.funct3) != funct3_names[instr.opcode].end()) {
                instr.name += " " + funct3_names[instr.opcode][instr.funct3];
            }
        } else {
            instr.name = "UNKNOWN";
        }
        
        return instr;
    }
    
    uint32_t execute_alu(const Instruction& instr, uint32_t val1, uint32_t val2, uint32_t pc) {
        switch (instr.type) {
            case InstrType::R_TYPE:
                switch (instr.funct3) {
                    case 0b000: // add/sub
                        if (instr.funct7 == 0b0000001) {
                            return static_cast<uint32_t>(static_cast<int32_t>(val1) - static_cast<int32_t>(val2));
                        } else {
                            return static_cast<uint32_t>(static_cast<int32_t>(val1) + static_cast<int32_t>(val2));
                        }
                    case 0b001: // sll
                        return val1 << (val2 & 0x1F);
                    case 0b010: // slt
                        return (static_cast<int32_t>(val1) < static_cast<int32_t>(val2)) ? 1 : 0;
                    case 0b011: // sltu
                        return (val1 < val2) ? 1 : 0;
                    case 0b100: // xor
                        return val1 ^ val2;
                    case 0b101: // srl/sra
                        if (instr.funct7 == 0b0100000) {
                            return static_cast<uint32_t>(static_cast<int32_t>(val1) >> (val2 & 0x1F));
                        } else {
                            return val1 >> (val2 & 0x1F);
                        }
                    case 0b110: // or
                        return val1 | val2;
                    case 0b111: // and
                        return val1 & val2;
                }
                break;
                
            case InstrType::I_TYPE:
                if (instr.opcode == 0b0010011) { // Arithmetic
                    switch (instr.funct3) {
                        case 0b000: // addi
                            return static_cast<uint32_t>(static_cast<int32_t>(val1) + instr.imm);
                        case 0b010: // slti
                            return (static_cast<int32_t>(val1) < instr.imm) ? 1 : 0;
                        case 0b011: // sltiu
                            return (val1 < static_cast<uint32_t>(instr.imm)) ? 1 : 0;
                        case 0b100: // xori
                            return val1 ^ static_cast<uint32_t>(instr.imm);
                        case 0b110: // ori
                            return val1 | static_cast<uint32_t>(instr.imm);
                        case 0b111: // andi
                            return val1 & static_cast<uint32_t>(instr.imm);
                        case 0b001: // slli
                            return val1 << (instr.imm & 0x1F);
                        case 0b101: // srli/srai
                            if ((instr.imm >> 5) & 0x20) {
                                return static_cast<uint32_t>(static_cast<int32_t>(val1) >> (instr.imm & 0x1F));
                            } else {
                                return val1 >> (instr.imm & 0x1F);
                            }
                    }
                }
                break;
                
            case InstrType::U_TYPE:
                if (instr.opcode == 0b0110111) { // lui
                    return static_cast<uint32_t>(instr.imm);
                } else if (instr.opcode == 0b0010111) { // auipc
                    return pc + static_cast<uint32_t>(instr.imm);
                }
                break;
                
            case InstrType::S_TYPE:
            case InstrType::B_TYPE:
                return val1 + static_cast<uint32_t>(instr.imm);
                
            case InstrType::J_TYPE:
                return pc + static_cast<uint32_t>(instr.imm);
                
            case InstrType::UNKNOWN:
                break;
        }
        
        return 0;
    }
    
    bool should_branch(const Instruction& instr, uint32_t val1, uint32_t val2) {
        if (instr.type != InstrType::B_TYPE) return false;
        
        switch (instr.funct3) {
            case 0b000: // beq
                return val1 == val2;
            case 0b001: // bne
                return val1 != val2;
            case 0b100: // blt
                return static_cast<int32_t>(val1) < static_cast<int32_t>(val2);
            case 0b101: // bge
                return static_cast<int32_t>(val1) >= static_cast<int32_t>(val2);
            case 0b110: // bltu
                return val1 < val2;
            case 0b111: // bgeu
                return val1 >= val2;
        }
        
        return false;
    }
    
    void write_memory(uint32_t address, uint32_t data, uint32_t funct3) {
        if (address >= MEMORY_SIZE) {
            throw runtime_error("Memory write out of bounds");
        }
        
        switch (funct3) {
            case 0b000: // sb
                memory[address] = data & 0xFF;
                break;
            case 0b001: // sh
                if (address + 1 >= MEMORY_SIZE) throw runtime_error("Memory write out of bounds");
                memory[address] = data & 0xFF;
                memory[address + 1] = (data >> 8) & 0xFF;
                break;
            case 0b010: // sw
                if (address + 3 >= MEMORY_SIZE) throw runtime_error("Memory write out of bounds");
                memory[address] = data & 0xFF;
                memory[address + 1] = (data >> 8) & 0xFF;
                memory[address + 2] = (data >> 16) & 0xFF;
                memory[address + 3] = (data >> 24) & 0xFF;
                break;
        }
    }
    
    uint32_t read_memory(uint32_t address, uint32_t funct3) {
        if (address >= MEMORY_SIZE) {
            throw runtime_error("Memory read out of bounds");
        }
        
        switch (funct3) {
            case 0b000: // lb
                {
                    int8_t val = static_cast<int8_t>(memory[address]);
                    return static_cast<uint32_t>(val);
                }
            case 0b001: // lh
                {
                    if (address + 1 >= MEMORY_SIZE) throw runtime_error("Memory read out of bounds");
                    int16_t val = static_cast<int16_t>(memory[address] | (memory[address + 1] << 8));
                    return static_cast<uint32_t>(val);
                }
            case 0b010: // lw
                {
                    if (address + 3 >= MEMORY_SIZE) throw runtime_error("Memory read out of bounds");
                    uint32_t val = memory[address] | (memory[address + 1] << 8) | 
                                  (memory[address + 2] << 16) | (memory[address + 3] << 24);
                    return val;
                }
            case 0b100: // lbu
                return static_cast<uint32_t>(memory[address]);
            case 0b101: // lhu
                {
                    if (address + 1 >= MEMORY_SIZE) throw runtime_error("Memory read out of bounds");
                    return static_cast<uint32_t>(memory[address] | (memory[address + 1] << 8));
                }
        }
        
        return 0;
    }
    
    void pipeline_step() {
        cycle_count++;
        
        // Writeback stage
        if (mem_wb.valid) {
            if (mem_wb.instr.type == InstrType::R_TYPE || 
                (mem_wb.instr.type == InstrType::I_TYPE && mem_wb.instr.opcode == 0b0010011) ||
                mem_wb.instr.type == InstrType::U_TYPE) {
                if (mem_wb.instr.rd != 0) { // x0 is hardwired to 0
                    regs[mem_wb.instr.rd] = mem_wb.alu_result;
                }
            } else if (mem_wb.instr.type == InstrType::J_TYPE ||
                      (mem_wb.instr.type == InstrType::I_TYPE && mem_wb.instr.opcode == 0b1100111)) {
                if (mem_wb.instr.rd != 0) {
                    regs[mem_wb.instr.rd] = mem_wb.pc + 4; // Return address
                }
            } else if (mem_wb.instr.type == InstrType::S_TYPE) {
                // Store instruction - no register writeback
            } else if (mem_wb.instr.opcode == 0b0000011) { // Load instruction
                if (mem_wb.instr.rd != 0) {
                    regs[mem_wb.instr.rd] = mem_wb.mem_data;
                }
            }
        }
        
        // Memory stage
        if (ex_mem.valid) {
            mem_wb = ex_mem;
            mem_wb.valid = true;
            
            if (ex_mem.instr.type == InstrType::S_TYPE) {
                write_memory(ex_mem.alu_result, ex_mem.val2, ex_mem.instr.funct3);
            } else if (ex_mem.instr.opcode == 0b0000011) { // Load instruction
                mem_wb.mem_data = read_memory(ex_mem.alu_result, ex_mem.instr.funct3);
            }
        } else {
            mem_wb.valid = false;
        }
        
        // Execute stage
        if (id_ex.valid) {
            ex_mem.instr = id_ex.instr;
            ex_mem.pc = id_ex.pc;
            ex_mem.val1 = id_ex.val1;
            ex_mem.val2 = id_ex.val2;
            ex_mem.valid = true;
            
            if (id_ex.instr.type == InstrType::B_TYPE) {
                // Branch instruction
                if (should_branch(id_ex.instr, id_ex.val1, id_ex.val2)) {
                    ex_mem.alu_result = id_ex.pc + id_ex.instr.imm;
                    flush = true; // Flush pipeline
                } else {
                    ex_mem.alu_result = id_ex.pc + 4;
                }
            } else if (id_ex.instr.type == InstrType::J_TYPE) {
                // Jump instruction
                ex_mem.alu_result = id_ex.pc + id_ex.instr.imm;
                flush = true; // Flush pipeline
            } else {
                // Regular ALU operation
                ex_mem.alu_result = execute_alu(id_ex.instr, id_ex.val1, id_ex.val2, id_ex.pc);
            }
        } else {
            ex_mem.valid = false;
        }
        
        // Instruction decode stage
        if (if_id.valid) {
            id_ex.instr = if_id.instr;
            id_ex.pc = if_id.pc;
            id_ex.valid = true;
            
            // Read register values
            id_ex.val1 = regs[if_id.instr.rs1];
            id_ex.val2 = regs[if_id.instr.rs2];
        } else {
            id_ex.valid = false;
        }
        
        // Instruction fetch stage
        if (!stall) {
            try {
                uint32_t raw_instr = fetch_instruction(pc);
                if_id.instr = decode_instruction(raw_instr);
                if_id.pc = pc;
                if_id.valid = true;
                
                // Check for unknown instruction - treat as NOP
                if (if_id.instr.type == InstrType::UNKNOWN) {
                    if_id.valid = false;
                    pc += 4;
                    return;
                }
                
                // Update PC for next instruction
                if (flush) {
                    // Handle branch/jump
                    if (ex_mem.valid && (ex_mem.instr.type == InstrType::B_TYPE || ex_mem.instr.type == InstrType::J_TYPE)) {
                        pc = ex_mem.alu_result;
                    } else {
                        pc += 4;
                    }
                    flush = false;
                } else {
                    pc += 4;
                }
                
                instruction_count++;
            } catch (const runtime_error& e) {
                // End of program or invalid instruction
                if_id.valid = false;
            }
        } else {
            stall = false;
        }
    }
    
    void run() {
        // Run until pipeline is empty
        bool running = true;
        while (running) {
            pipeline_step();
            
            // Check if pipeline is empty
            if (!if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid) {
                running = false;
            }
            
            // Safety limit to prevent infinite loops
            if (cycle_count > 1000000) {
                cerr << "Error: Cycle limit exceeded" << endl;
                break;
            }
        }
    }
    
    void print_state() const {
        cout << "=== RISC-V Simulator State ===" << endl;
        cout << "Cycles: " << cycle_count << endl;
        cout << "Instructions: " << instruction_count << endl;
        cout << "PC: 0x" << hex << setw(8) << setfill('0') << pc << dec << endl;
        
        cout << "Registers:" << endl;
        for (int i = 0; i < NUM_REGS; i++) {
            if (i % 4 == 0) cout << "x" << setw(2) << i << ":";
            cout << " 0x" << hex << setw(8) << setfill('0') << regs[i] << dec;
            if (i % 4 == 3) cout << endl;
        }
        cout << endl;
    }
    
    // Return value for OJ (as specified in problem description)
    uint32_t get_return_value() const {
        // Return the value in register x10 (a0) as the program result
        return regs[10];
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

// Read hex data from stdin (alternative format)
vector<uint8_t> read_hex_data() {
    vector<uint8_t> data;
    string line;
    
    while (getline(cin, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Try to parse as hex
        try {
            // Remove any whitespace
            line.erase(remove_if(line.begin(), line.end(), ::isspace), line.end());
            
            // Parse hex string
            for (size_t i = 0; i < line.length(); i += 2) {
                if (i + 1 < line.length()) {
                    string byte_str = line.substr(i, 2);
                    uint8_t byte = static_cast<uint8_t>(stoi(byte_str, nullptr, 16));
                    data.push_back(byte);
                }
            }
        } catch (...) {
            // If hex parsing fails, try to parse as decimal integers
            istringstream iss(line);
            uint32_t val;
            while (iss >> val) {
                // Add 4 bytes for each 32-bit value in little endian
                data.push_back(val & 0xFF);
                data.push_back((val >> 8) & 0xFF);
                data.push_back((val >> 16) & 0xFF);
                data.push_back((val >> 24) & 0xFF);
            }
        }
    }
    
    return data;
}

int main() {
    try {
        // Try to read program data from stdin
        vector<uint8_t> program_data = read_input_data();
        
        // If binary reading fails or gives no data, try hex format
        if (program_data.empty()) {
            // Clear stdin and try again
            cin.clear();
            cin.seekg(0);
            program_data = read_hex_data();
        }
        
        if (program_data.empty()) {
            cerr << "Error: No input data provided" << endl;
            return 1;
        }
        
        // Create and initialize simulator
        RISCVSimulator simulator;
        simulator.load_program(program_data);
        
        // Run simulation
        simulator.run();
        
        // Output result (return value as specified in problem description)
        uint32_t result = simulator.get_return_value();
        cout << result << endl;
        
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}