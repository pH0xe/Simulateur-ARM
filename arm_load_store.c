/*
Armator - simulateur de jeu d'instruction ARMv5T � but p�dagogique
Copyright (C) 2011 Guillaume Huard
Ce programme est libre, vous pouvez le redistribuer et/ou le modifier selon les
termes de la Licence Publique G�n�rale GNU publi�e par la Free Software
Foundation (version 2 ou bien toute autre version ult�rieure choisie par vous).

Ce programme est distribu� car potentiellement utile, mais SANS AUCUNE
GARANTIE, ni explicite ni implicite, y compris les garanties de
commercialisation ou d'adaptation dans un but sp�cifique. Reportez-vous � la
Licence Publique G�n�rale GNU pour plus de d�tails.

Vous devez avoir re�u une copie de la Licence Publique G�n�rale GNU en m�me
temps que ce programme ; si ce n'est pas le cas, �crivez � la Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
�tats-Unis.

Contact: Guillaume.Huard@imag.fr
	 B�timent IMAG
	 700 avenue centrale, domaine universitaire
	 38401 Saint Martin d'H�res
*/
#include "arm_load_store.h"
#include "arm_exception.h"
#include "arm_constants.h"
#include "arm_instruction.h"
#include "util.h"
#include "debug.h"

/* Instructions prises en compte :
 * Simple :
 *      LDR - (A4-43)   27-26 = 1 && 22 = 0 && 20 = 1               - loads a word from a memory address
 *      LDRB - (A4-46)  27-26 = 1 && 22 = 1 && 20 = 1               - loads a byte from memory and zero-extends the byte to a 32-bit word
 *      LDRH - (A4-54)  27-25 = 0           && 20 = 1 && 7-4 = B    - loads a halfword from memory and zero-extends it to a 32-bit word.
 *      STR - (A4-193)  27-26 = 1 && 22 = 0 && 20 = 0
 *      STRB - (A4-195) 27-26 = 1 && 22 = 1 && 20 = 0
 *      STRH - (A4-204) 27-25 = 0           && 20 = 0 && 7-4 = B    - stores a halfword from the least significant halfword of a register to memory.
 * Multiple :
 *      LDM(1) - (A4-36) 27-25 = 4 && 22 = 0 && 20 = 1
 *      STM(1) - (A4-189) 27-25 = 4 && 22 = 0 && 20 = 0
 *
 */

int numberOfSetBits(uint16_t champ) {
    int count = 0;
    for (int i = 0; i < 16; i++) {
        if (get_bit(champ, i) == 1){
            count++;
        }
    }
    return count;
}

uint32_t shift(arm_core p, uint32_t ins, uint32_t valRm) {
    int shift_type = get_bits(ins, 6,5);
    uint8_t shift_value = get_bits(ins, 11, 7);
    uint32_t index;

    switch (shift_type) {
        case 0:
            index = valRm << shift_value;
            break;
        case 1:
            if (shift_value == 0 ){
                index = 0;
            } else {
                index = valRm >> shift_value;
            }
            break;
        case 2:
            if (shift_value == 0) {
                if (get_bit(valRm, 31) == 1) {
                    index = 0xFFFFFFFF;
                } else {
                    index = 0;
                }
            } else {
                index = asr(valRm, shift_value);
            }
            break;
        case 3:
            if (shift_value == 0) {
                index = (get_bit(arm_read_cpsr(p), C) << 31) | (valRm >> 1);
            }
            else {
                index = ror(valRm, shift_value);
            }
            break;
        default:
            return -1;
    }
    return index;
}

uint32_t getAddressModeHalf(arm_core p, uint32_t ins) {
    int is_post = get_bit(ins, 24) == 0;
    uint32_t type = get_bits(ins, 22, 21);
    int u = get_bit(ins, 23);
    uint32_t rn = get_bits(ins, 19, 16);
    uint32_t valRn = arm_read_register(p, rn);

    uint8_t HOffset = get_bits(ins, 11, 8);
    uint8_t LOffset = get_bits(ins, 3, 0);

    if (is_post && type == 0) {
        // register post index
        uint32_t address = valRn;
        if (condition(p, ins) == 1) {
            uint32_t valRm = arm_read_register(p, LOffset);
            valRn = u == 1 ? valRn + valRm : valRn - valRm;
            arm_write_register(p, rn, valRn);
            return address;
        }
        return -1;
    } else if (is_post && type == 0x2) {
        // imm post indexed
        uint32_t address = valRn;
        if (condition(p, ins) == 1) {
            uint8_t offset_8 = (HOffset << 4) | LOffset;
            valRn = u == 1 ? valRn + offset_8 : valRn - offset_8;
            arm_write_register(p, rn, valRn);
            return address;
        }
        return -1;
    } else if (!is_post && type == 0){
        // register offset
        uint32_t valRm = arm_read_register(p, LOffset);
        return u == 1 ? valRn + valRm : valRn - valRm;
    } else if (!is_post && type == 0x1){
        // register pre indexed
        uint32_t valRm = arm_read_register(p, LOffset);
        uint32_t address = u == 1 ? valRn + valRm : valRn - valRm;

        if (condition(p, ins) == 1) {
            arm_write_register(p, rn, address);
            return address;
        } else return -1;
    } else if (!is_post && type == 0x2){
        // imm offset
        uint8_t offset_8 = (HOffset << 4) | LOffset;
        return u == 1 ? valRn + offset_8 : valRn - offset_8;
    } else if (!is_post && type == 0x3){
        // imm pre indexed
        uint8_t offset_8 = (HOffset << 4) | LOffset;
        uint32_t address = u == 1 ? valRn + offset_8 : valRn - offset_8;

        if (condition(p, ins) == 1) {
            arm_write_register(p, rn, address);
            return address;
        } else return -1;
    }
    return -1;
}

uint32_t getAddressModeBW(arm_core p, uint32_t ins) {
    int u = get_bit(ins, 23);
    uint32_t rn = get_bits(ins, 19, 16);
    uint32_t valRn = arm_read_register(p, rn);

    int is_imm = get_bit(ins, 25) == 0;

    if (is_imm) {
        uint16_t offset12 = get_bits(ins, 11, 0);
        if (get_bit(ins,21) == 0 && get_bit(ins, 24) == 1){
            //  Immediate offset
            if (u == 1) {
                return valRn + offset12;
            } else {
                return valRn - offset12;
            }
        } else if (get_bit(ins,21) == 1 && get_bit(ins, 24) == 1) {
            // immediate pre indexed
            uint32_t address;
            if (u == 1) {
                address = valRn + offset12;
            } else {
                address = valRn - offset12;
            }

            if (condition(p, ins) == 1) {
                arm_write_register(p, rn, address);
                return address;
            } else return -1;

        }else if (get_bit(ins,21) == 0 && get_bit(ins, 24) == 0) {
            // immediate post indexed
            if (condition(p, ins)) {
                if (u == 1){
                    return valRn + offset12;
                } else {
                    return valRn - offset12;
                }
            }
        }
        return -1;
    } else {
        if (get_bit(ins, 24) == 0) {
            // register post-indexed
            uint32_t valRm = arm_read_register(p, get_bits(ins, 3,0));
            uint32_t index = shift(p, ins, valRm);
            if (condition(p, ins)) {
                if (u == 1) {
                    return valRn + index;
                } else {
                    return valRn - index;
                }
            }
        } else if (get_bit(ins,21) == 1){
            // register pre-indexed
            uint32_t valRm = arm_read_register(p, get_bits(ins, 3,0));
            uint32_t index = shift(p, ins, valRm);
            uint32_t address;
            if (u == 1){
                 address = valRn + index;
            } else {
                address = valRn - index;
            }

            if (condition(p, ins) == 1) {
                arm_write_register(p, rn, address);
                return address;
            } else return -1;

        } else if (get_bit(ins,21) == 0){
            // register offset
            uint32_t valRm = arm_read_register(p, get_bits(ins, 3,0));
            uint32_t index = shift(p, ins, valRm);

            if (u == 1) {
                return valRn + index;
            } else {
                return valRn - index;
            }
        }
        return -1;
    }
}

void getAddressModeMulti(arm_core p, uint32_t ins, uint32_t* addresses){
    uint32_t rn = get_bits(ins, 19, 16);
    uint32_t valRn = arm_read_register(p, rn);
    uint32_t start_address, end_address;

    int after = get_bit(ins, 24) == 0;
    int inc = get_bit(ins, 23) == 1;
    int count = numberOfSetBits(get_bits(ins, 15, 0)) * 4;

    if (inc && after){
        // Increment after
        start_address = valRn;
        end_address = valRn + count - 4;
        if (condition(p, ins) && get_bit(ins, 21) == 1){
            arm_write_register(p, rn, valRn+count);
        }
    } else if (inc && !after) {
        // increment before
        start_address = valRn + 4;
        end_address = valRn + count;
        if (condition(p, ins) && get_bit(ins, 21) == 1){
            arm_write_register(p, rn, valRn+count);
        }
    } else if (!inc && after) {
        //decrement after A5-45
        start_address = valRn - count + 4;
        end_address = valRn;
        if (condition(p, ins) && get_bit(ins, 21) == 1){
            arm_write_register(p, rn, valRn - count);
        }
    } else if (!inc && !after) {
        // decrement before
        start_address = valRn - count;
        end_address = valRn - 4;
        if (condition(p, ins) && get_bit(ins, 21) == 1){
            arm_write_register(p, rn, valRn - count);
        }

    }
    addresses[0] = start_address;
    addresses[1] = end_address;
}

int arm_load_store(arm_core p, uint32_t ins) {
    int is_load = get_bit(ins, 20) == 1;
    int is_H = get_bits(ins, 27, 25) == 0 && get_bits(ins, 7, 4) == 0xB;
    int is_B = get_bit(ins, 22) == 1;
    int is_S = get_bits(ins, 27, 25) == 0 && get_bits(ins, 7, 4) >= 0xE;
    uint32_t reg_dest = get_bits(ins, 15, 12);

    if (is_load) {
        if (is_H){
            // LDRH/LDRSH
            uint32_t address = getAddressModeHalf(p, ins);
            if (address != -1 ){
                uint16_t value;
                arm_read_half(p, address, &value);
                uint32_t value_32 = value;
                if (is_S) {
                    if (get_bit(value,15)) value_32 = set_bits(value_32, 30, 16, 0xFFFF);
                }
                arm_write_register(p, reg_dest, value_32);
            }
        } else if (is_B) {
            // LDRB/LDRSB
            uint32_t address = getAddressModeBW(p, ins);
            if (address != -1) {
                uint8_t value;
                arm_read_byte(p, address, &value);
                uint32_t value_32 = value;
                if (is_S) {
                    if (get_bit(value, 7)) value_32 = set_bits(value_32, 30, 8, 0xFFFFFF);
                }
                arm_write_register(p, reg_dest, value_32);
            }
        } else {
            // LDR
            uint32_t address = getAddressModeBW(p, ins);
            if (address != -1) {
                uint32_t value;
                arm_read_word(p, address, &value);
                arm_write_register(p, reg_dest, value);
            }
        }
    } else {
        if (is_H){
            // STRH
            uint32_t address = getAddressModeHalf(p, ins);
            uint16_t value = arm_read_register(p, reg_dest);
            arm_write_half(p,address, value);
        } else if (is_B) {
            // STRB
            uint32_t address = getAddressModeBW(p, ins);
            uint8_t value = arm_read_register(p, reg_dest);
            arm_write_byte(p,address, value);
        } else {
            // STR
            uint32_t address = getAddressModeBW(p, ins);
            uint32_t value = arm_read_register(p, reg_dest);
            arm_write_word(p,address, value);
        }
    }
    return 0;
}

int arm_load_store_multiple(arm_core p, uint32_t ins) {
    int is_load = get_bit(ins, 20) == 1;
    uint32_t addresses[2];
    addresses[0] = -1;
    addresses[1] = -1;
    getAddressModeMulti(p, ins, addresses);
    if (addresses[0] == -1 || addresses[1] == -1) {
        return -1;
    }
    if (is_load){
        // LDM(1)
        if (get_bit(ins, 22) == 0){
            if (condition(p, ins)){
                uint32_t address = addresses[0];

                for (int i = 0; i < 15; i++) {
                    if (get_bit(ins, i) == 1) {
                        uint32_t value;
                        arm_read_word(p, address, &value);
                        arm_write_register(p, i, value);
                        address += 4;
                    }
                }

                if (get_bit(ins, 15) == 1) {
                    uint32_t value;
                    arm_read_word(p, address, &value);

                    arm_write_register(p, 15, value & 0xFFFFFFFE);
                    address += 4;
                }

                if (addresses[1] != address - 4) {
                    return -1;
                }
            }
        }
    } else {
        //STM (1)
        if (get_bit(ins, 22) == 0){
            if (condition(p, ins)) {
                uint32_t address = addresses[0];

                for (int i = 0; i < 16; i++) {
                    if (get_bit(ins, i) == 1) {
                        arm_write_word(p, address, arm_read_register(p, i));
                        address += 4;
                    }
                }

                if (addresses[1] != address - 4) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

int arm_coprocessor_load_store(arm_core p, uint32_t ins) {
    /* Not implemented */
    return UNDEFINED_INSTRUCTION;
}
