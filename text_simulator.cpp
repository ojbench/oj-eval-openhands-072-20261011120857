#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

using namespace std;

// Text-based RISC-V simulator
class TextRISCVSimulator {
private:
    static const int NUM_REGS = 32;
    uint32_t regs[NUM_REGS];
    
public:
    TextRISCVSimulator() {
        memset(regs, 0, sizeof(regs));
    }
    
    void execute_instruction(const string& instr_str) {
        istringstream iss(instr_str);
        string opcode;
        iss >> opcode;
        
        if (opcode == "addi") {
            string rd_str, rs1_str, imm_str;
            iss >> rd_str >> rs1_str >> imm_str;
            
            int rd = stoi(rd_str.substr(1)); // Remove 'x' prefix
            int rs1 = stoi(rs1_str.substr(1));
            int imm = stoi(imm_str);
            
            if (rd != 0) {
                regs[rd] = regs[rs1] + imm;
            }
        } else if (opcode == "add") {
            string rd_str, rs1_str, rs2_str;
            iss >> rd_str >> rs1_str >> rs2_str;
            
            int rd = stoi(rd_str.substr(1));
            int rs1 = stoi(rs1_str.substr(1));
            int rs2 = stoi(rs2_str.substr(1));
            
            if (rd != 0) {
                regs[rd] = regs[rs1] + regs[rs2];
            }
        } else if (opcode == "li") {
            string rd_str, imm_str;
            iss >> rd_str >> imm_str;
            
            int rd = stoi(rd_str.substr(1));
            int imm = stoi(imm_str);
            
            if (rd != 0) {
                regs[rd] = imm;
            }
        } else if (opcode == "mv") {
            string rd_str, rs1_str;
            iss >> rd_str >> rs1_str;
            
            int rd = stoi(rd_str.substr(1));
            int rs1 = stoi(rs1_str.substr(1));
            
            if (rd != 0) {
                regs[rd] = regs[rs1];
            }
        }
    }
    
    uint32_t get_return_value() const {
        return regs[10]; // a0
    }
    
    void print_registers() const {
        for (int i = 0; i < NUM_REGS; i++) {
            cout << "x" << i << " = " << regs[i] << endl;
        }
    }
};

int main() {
    try {
        TextRISCVSimulator simulator;
        string line;
        
        while (getline(cin, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;
            
            simulator.execute_instruction(line);
        }
        
        uint32_t result = simulator.get_return_value();
        cout << result << endl;
        
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}