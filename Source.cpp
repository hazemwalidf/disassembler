#include <iostream>
#include <fstream>
#include <string>
#include "stdlib.h"
#include <iomanip>

using namespace std;

int regs[32] = { 0 };
unsigned int pc = 0x0;

char memory[8 * 1024];	// only 8KB of memory located at address 0

bool exitFlag = false;

void emitError(char* s)
{
	cout << s;
	exit(0);
}

void printPrefix(unsigned int instA, unsigned int instW) {
	cout << "0x" << hex << std::setfill('0') << std::setw(8) << instA << "\t0x" << std::setw(8) << instW;
}
void instDecExec(unsigned int instWord)
{
	unsigned int rd, rs1, rs2, funct3, funct7, opcode, shamt;
	unsigned int I_imm, S_imm, B_imm, U_imm, J_imm;

	unsigned int address, half;

	unsigned int instPC = pc - 4;

	unsigned int t1, t2;

	opcode = instWord & 0x0000007F;
	rd = (instWord >> 7) & 0x0000001F;
	funct3 = (instWord >> 12) & 0x00000007;
	rs1 = (instWord >> 15) & 0x0000001F;
	rs2 = (instWord >> 20) & 0x0000001F;
	funct7 = (instWord >> 25) & 0x0000007F;

	shamt = (instWord >> 20) & 0x0000001F;
	S_imm = (instWord >> 7 & 0x1F) | ((instWord >> 25 & 0x7F) << 5);
	// B_imm construction
	unsigned int imm4 = (instWord >> 8 & 0xF);
	unsigned int imm6 = (instWord >> 25 & 0x3F) << 4;
	unsigned int imm11 = (instWord >> 7 & 0x1) << 10;
	unsigned int imm12 = (instWord >> 31 & 0x1) << 11;
	B_imm = (imm4 | imm6 | imm11 | imm12) << 1; // we need to multiply by 2 after construction as per datasheet
	if ((B_imm >> 12) & 0x1 == 1) // if last bit from left means -ve
	{
		B_imm = B_imm | 0xFFFFE000;
	}
	U_imm = instWord & 0xFFFFF000;
	// J_Imm construction
	unsigned int imm10 = (instWord >> 21 & 0x3FF);
	imm11 = (instWord >> 20 & 0x1) << 10;
	imm12 = (instWord >> 12 & 0xFF) << 11;
	unsigned int imm20 = (instWord >> 31 & 0x1) << 19;
	J_imm = (imm10 | imm11 | imm12 | imm20) << 1; // we need to multiply by 2 after construction as per datasheet
	if ((J_imm >> 20) & 0x1 == 1) // means -ve
	{
		J_imm = J_imm | 0xFFE00000;
	}
	// — inst[31] — inst[30:25] inst[24:21] inst[20]
	I_imm = ((instWord >> 20) & 0x7FF) | (((instWord >> 31) ? 0xFFFFF800 : 0x0));

	printPrefix(instPC, instWord);

	if (opcode == 0x33) {		// R Instructions
		switch (funct3) {
		case 0:				// 0 in func3 means ADD or SUB 
			if (funct7 == 32)// Sub
			{
				cout << "\tSUB\tx" << rd << ", x" << rs1 << ", x" << rs2 << "\n";
				regs[rd] = regs[rs1] - regs[rs2];
			}
			else // otherwise Add
			{
				cout << "\tADD\tx" << rd << ", x" << rs1 << ", x" << rs2 << "\n";
				regs[rd] = regs[rs1] + regs[rs2];
			}
			break;

		case 1:				// 1 in func3 means SLL
			cout << "\tSLL\tx" << rd << ", x" << rs1 << ", x" << rs2 << "\n";
			regs[rd] = regs[rs1] << regs[rs2];
			break;

		case 2:				// 2 in func3 means SLT
			cout << "\tSLT\tx" << rd << ", x" << rs1 << ", x" << rs2 << "\n";
			if (regs[rs1] < regs[rs2])
				regs[rd] = 1;
			else
				regs[rd] = 0;
			break;

		case 3:				// 3 in func3 means SLTU
			cout << "\tSLTU\tx" << rd << ", x" << rs1 << ", x" << rs2 << "\n";
			t1 = regs[rs1];
			t2 = regs[rs2];
			if (t1 < t2)
				regs[rd] = 1;
			else
				regs[rd] = 0;
			break;

		case 4:				// 4 in func3 means XOR
			cout << "\tXOR\tx" << rd << ", x" << rs1 << ", x" << rs2 << "\n";
			regs[rd] = regs[rs1] ^ regs[rs2];
			break;

		case 5:				// 5 in func3 means SRL or SRA 
			if (funct7 == 32)// SRA
			{
				cout << "\tSRA\tx" << rd << ", x" << rs1 << ", x" << rs2 << "\n";
				regs[rd] = regs[rs1] >> regs[rs2];
			}
			else // otherwise SRL
			{
				cout << "\tSRL\tx" << rd << ", x" << rs1 << ", x" << rs2 << "\n";
				regs[rd] = regs[rs1] >> regs[rs2];
			}
			break;

		case 6:				// 6 in func3 means OR
			cout << "\tOR\tx" << rd << ", x" << rs1 << ", x" << rs2 << "\n";
			regs[rd] = regs[rs1] | regs[rs2];
			break;

		case 7:				// 7 in func3 means AND
			cout << "\tAND\tx" << rd << ", x" << rs1 << ", x" << rs2 << "\n";
			regs[rd] = regs[rs1] & regs[rs2];
			break;

		default:
			cout << "\tUnkown R Instruction \n";
		}
	}
	else if (opcode == 0x3) {	// I instructions - Part1
		switch (funct3) {
		case 0:
			cout << "\tLB\tx" << rd << ", " << hex << "0x" << (int)I_imm << "(x" << rs1 << ")" << "\n";
			address = regs[rs1] + (int)I_imm;
			if (memory[address] >> 7 & 0x1 == 1) // i need to extend it with -ve values 
			{
				regs[rd] = memory[address] | 0xFFFFFF00;
			}
			else
			{
				regs[rd] = memory[address];
			}
			break;

		case 1:
			cout << "\tLH\tx" << rd << ", " << hex << "0x" << (int)I_imm << "(x" << rs1 << ")" << "\n";
			address = regs[rs1] + (int)I_imm;
			// least significant byte is memory[address]
			half = memory[address + 1] << 8 | memory[address];

			if (half >> 15 & 0x1 == 1) // i need to extend it with -ve values 
			{
				regs[rd] = half | 0xFFFF0000;
			}
			else
			{
				regs[rd] = half;
			}
			break;

		case 2:
			// read 4 bytes to represent a word of 32 bits
			cout << "\tLW\tx" << rd << ", " << hex << "0x" << (int)I_imm << "(x" << rs1 << ")" << "\n";
			address = regs[rs1] + (int)I_imm;
			regs[rd] = (memory[address + 3] << 24) | (memory[address + 2] << 16) | (memory[address + 1] << 8) | memory[address];
			break;

		case 4:
			// LBU just like LB however no need to sign extend
			cout << "\tLBU\tx" << rd << ", " << hex << "0x" << (int)I_imm << "(x" << rs1 << ")" << "\n";
			address = regs[rs1] + (int)I_imm;
			regs[rd] = memory[address];
			break;
		case 5:
			cout << "\tLHU\tx" << rd << ", " << hex << "0x" << (int)I_imm << "(x" << rs1 << ")" << "\n";
			address = regs[rs1] + (int)I_imm;
			regs[rd] = memory[address + 1] << 8 | memory[address];
			break;



		default:
			cout << "\tUnkown I Instruction \n";
		}
	}
	else if (opcode == 0x13) {	// I instructions - Part2
		switch (funct3) {
		case 0:
			cout << "\tADDI\tx" << rd << ", x" << rs1 << ", " << hex << "0x" << (int)I_imm << "\n";
			regs[rd] = regs[rs1] + (int)I_imm;
			break;

		case 2:
			cout << "\tSLTI\tx" << rd << ", x" << rs1 << ", " << hex << "0x" << (int)I_imm << "\n";
			if (regs[rs1] < (int)I_imm)
				regs[rd] = 1;
			else
				regs[rd] = 0;
			break;

		case 3:
			cout << "\tSLTIU\tx" << rd << ", x" << rs1 << ", " << hex << "0x" << (int)I_imm << "\n";
			if ((unsigned int)regs[rs1] < (unsigned int)I_imm)
				regs[rd] = 1;
			else
				regs[rd] = 0;
			break;

		case 4:
			cout << "\tXORI\tx" << rd << ", x" << rs1 << ", " << hex << "0x" << (int)I_imm << "\n";
			regs[rd] = regs[rs1] ^ (int)I_imm;
			break;

		case 6:
			cout << "\tORI\tx" << rd << ", x" << rs1 << ", " << hex << "0x" << (int)I_imm << "\n";
			regs[rd] = regs[rs1] | (int)I_imm;
			break;

		case 7:
			cout << "\tANDI\tx" << rd << ", x" << rs1 << ", " << hex << "0x" << (int)I_imm << "\n";
			regs[rd] = regs[rs1] & (int)I_imm;
			break;

		case 1:
			cout << "\tSLLI\tx" << rd << ", x" << rs1 << ", " << hex << "0x" << (int)I_imm << "\n";
			regs[rd] = regs[rs1] << (int)shamt;
			break;

		case 5:
			if (funct7 == 32)
			{
				cout << "\tSRAI\tx" << rd << ", x" << rs1 << ", " << hex << "0x" << (int)I_imm << "\n";
				regs[rd] = regs[rs1] >> (int)shamt;
			}
			else
			{
				cout << "\tSRLI\tx" << rd << ", x" << rs1 << ", " << hex << "0x" << (int)I_imm << "\n";
				regs[rd] = regs[rs1] >> (int)shamt;
			}

			break;

		default:
			cout << "\tUnkown I Instruction \n";
		}
	}
	else if (opcode == 0x23) { // S-Format
		if (funct3 == 0) { // SB
			cout << "\tSB\tx" << rs2 << ", " << hex << "0x" << (int)S_imm << "(x" << rs1 << ")" << "\n";
			address = regs[rs1] + (int)S_imm;
			memory[address] = regs[rs2];
		}
		else if (funct3 == 1)
		{
			cout << "\tSH\tx" << rs2 << ", " << hex << "0x" << (int)S_imm << "(x" << rs1 << ")" << "\n";
			address = regs[rs1] + (int)S_imm;
			memory[address] = regs[rs2] & 0xFF;
			memory[address + 1] = (regs[rs2] >> 8) & 0xFF;
		}
		else if (funct3 == 2)
		{
			cout << "\tSW\tx" << rs2 << ", " << hex << "0x" << (int)S_imm << "(x" << rs1 << ")" << "\n";
			address = regs[rs1] + (int)S_imm;
			memory[address] = regs[rs2] & 0xFF;
			memory[address + 1] = (regs[rs2] >> 8) & 0xFF;
			memory[address + 2] = (regs[rs2] >> 16) & 0xFF;
			memory[address + 3] = (regs[rs2] >> 24) & 0xFF;
		}
	}
	else if (opcode == 0x63) { // B-Format
		if (funct3 == 0) { // BEQ
			cout << "\tBEQ\tx" << rs1 << ", x" << rs2 << ", " << hex << "0x" << (int)B_imm << "\n";
			if (regs[rs1] == regs[rs2])
				pc = instPC + B_imm;
		}
		else if (funct3 == 1) { // BNE
			cout << "\tBNE\tx" << rs1 << ", x" << rs2 << ", " << hex << "0x" << (int)B_imm << "\n";
			if (regs[rs1] != regs[rs2])
				pc = instPC + B_imm;
		}
		else if (funct3 == 4) { // BLT
			cout << "\tBLT\tx" << rs1 << ", x" << rs2 << ", " << hex << "0x" << (int)B_imm << "\n";
			if (regs[rs1] < regs[rs2])
				pc = instPC + B_imm;
		}
		else if (funct3 == 5) { // BGE
			cout << "\tBGE\tx" << rs1 << ", x" << rs2 << ", " << hex << "0x" << (int)B_imm << "\n";
			if (regs[rs1] >= regs[rs2]) {
				pc = instPC + B_imm;
			}
		}
		else if (funct3 == 6) { // BLTU
			cout << "\tBLTU\tx" << rs1 << ", x" << rs2 << ", " << hex << "0x" << (int)B_imm << "\n";
			if ((unsigned int)regs[rs1] < (unsigned int)regs[rs2])
				pc = instPC + B_imm;
		}
		else if (funct3 == 7) { // BGEU
			cout << "\tBGEU\tx" << rs1 << ", x" << rs2 << ", " << hex << "0x" << (int)B_imm << "\n";
			if ((unsigned int)regs[rs1] >= (unsigned int)regs[rs2])
				pc = instPC + B_imm;
		}
	}
	else if (opcode == 0x37) {// U-Format LUI
		cout << "\tLUI\tx" << rd << ", 0x" << (int)U_imm << "\n";
		regs[rd] = U_imm;
	}
	else if (opcode == 0x17) {// U-Format AUIPC
		cout << "\tAUIPC\tx" << rd << ", 0x" << (int)U_imm << "\n";
		regs[rd] = U_imm + instPC;
	}
	else if (opcode == 0x6F) {// J-Format 

		regs[rd] = pc;
		pc = instPC + J_imm;
		cout << "\tJAL\t" << pc << "\n";
	}
	else if (opcode == 0x67) {// JALR Format 

		regs[rd] = pc;
		pc = regs[rs1] + I_imm;
		cout << "\tJALR\t" << pc << "\n";
	}
	else if (opcode == 0x73) {
		cout << "\tecall\n";
		if (regs[17] == 1) // 1 in a7 means print integer
		{
			// it will print the data inside a0, which is at index 10 in registers array
			cout << "\t\t\tOutput : " << regs[10] << endl;
		}
		else if (regs[17] == 5) // 5 in a7 means read integer
		{
			// it will take the data into a0, which is at index 10 in registers array
			cin >> regs[10];
		}
		else if (regs[17] == 4) // 4 in a7 means print string
		{
			cout << "\t\t\tOutput : ";
			address = regs[10];
			while (memory[address] != '0')
			{
				cout << (char)memory[address];
				address++;
			}
			cout << endl;
		}
		else if (regs[17] == 10) // 10 in a7 means exit
		{
			exitFlag = true;
		}
		else if (regs[17] == 8) // 8 in a7 means read string
		{
			string line;
			getline(cin, line);

			address = regs[10];
			int i;
			for (i = 0; i < line.length(); i++)
			{
				memory[address + i] = line[i];
			}
			memory[address + i] = '0';
		}
	}
	regs[0] = 0;
}

int main(int argc, char* argv[]) {

	unsigned int instWord = 0;
	ifstream inFile;
	ofstream outFile;

	char a[] = "use: rv32i_sim <machine_code_file_name>\n";
	if (argc < 1) emitError(a);

	inFile.open(argv[1], ios::in | ios::binary | ios::ate);

	if (inFile.is_open())
	{
		int fsize = inFile.tellg();

		inFile.seekg(0, inFile.beg);

		char b[] = "Cannot read from input file\n";
		if (!inFile.read((char*)memory, fsize)) emitError(b);

		while (true) {
			instWord = (unsigned char)memory[pc] |
				(((unsigned char)memory[pc + 1]) << 8) |
				(((unsigned char)memory[pc + 2]) << 16) |
				(((unsigned char)memory[pc + 3]) << 24);
			pc += 4;

			if (pc >= 0x1000)
			{
				break;
			}

			if (exitFlag == true)
				break;			// stop when exit flag becomes true

			instDecExec(instWord);
		}

		// dump the registers
		for (int i = 0;i < 32;i++)
			cout << "x" << dec << i << ": \t" << "0x" << hex << std::setfill('0') << std::setw(8) << regs[i] << "\n";

	}
	
	else
	{
		char c[] = "Cannot access input file\n";
		emitError(c);
	}
}
