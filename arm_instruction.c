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
#include "arm_instruction.h"
#include "arm_exception.h"
#include "arm_data_processing.h"
#include "arm_load_store.h"
#include "arm_branch_other.h"
#include "arm_constants.h"
#include "util.h"

int condition(arm_core p, uint32_t ins) {
    uint8_t cond = get_bits(ins,31,28);
    uint32_t cpsr_value=arm_read_cpsr(p);
    switch (cond){
        case 0:     //EQ
            if(get_bit(cpsr_value,Z)) return 1;
            break;
        case 1:     //NE
            if(!get_bit(cpsr_value,Z)) return 1;
            break;
        case 2:     //CS/HS
            if(get_bit(cpsr_value,C)) return 1;
            break;
        case 3:     //CC/LO
            if(!get_bit(cpsr_value,C)) return 1;
            break;
        case 4:     //MI
            if(get_bit(cpsr_value,N)) return 1;
            break;
        case 5:     //PL
            if(!get_bit(cpsr_value,N)) return 1;
            break;
        case 6:     //VS
            if(get_bit(cpsr_value,V)) return 1;
            break;
        case 7:     //VC
            if(!get_bit(cpsr_value,V)) return 1;
            break;
        case 8:     //HI
            if(get_bit(cpsr_value,C) && !get_bit(cpsr_value,Z)) return 1;
            break;
        case 9:     //LS
            if(!get_bit(cpsr_value,C) || get_bit(cpsr_value,Z)) return 1;
            break;
        case 10:    //GE
            if(get_bit(cpsr_value,N) == get_bit(cpsr_value,V)) return 1;
            break;
        case 11:    //LT
            if(get_bit(cpsr_value,N) != get_bit(cpsr_value,V)) return 1;
            break;
        case 12:    //GT
            if((get_bit(cpsr_value,N) == get_bit(cpsr_value,V)) && !get_bit(cpsr_value,Z)) return 1;
            break;
        case 13:    //LE
            if((get_bit(cpsr_value,N) != get_bit(cpsr_value,V)) || get_bit(cpsr_value,Z)) return 1;
            break;
        case 14:    //AL
            return 1;
            break;
        default:    //ERROR
            return 0;
            break;
    }
    return 0;
}

static int arm_execute_instruction(arm_core p) {
    uint32_t val;
    uint8_t champ;
    arm_fetch(p, &val);
    if (((val & 0xF0000000) >> 28) == 0xF){
        return -1;
    } 
    champ = (uint8_t)((val & 0x0E000000) >> 25);
    switch (champ){
        case 0:         //Data processing & load store
            if (get_bit(val, 4) == 1 && get_bit(val, 7) == 1) {
                return arm_load_store(p, val);
            }
            else if (((get_bits(val,24,23) == 2) && (get_bit(val,20) == 0)) && (get_bit(val,4) == 0 || (get_bit(val,7) == 0 && get_bit(val,4) == 1))){
                return arm_miscellaneous(p, val);
            }
            else if (condition(p, val))
                return arm_data_processing_shift(p, val);
            break;
        case 1:         //Data processing
            if (condition(p, val))
                return arm_data_processing_shift(p, val);
            break;
        case 2:         //Load/Store
            return arm_load_store(p, val);
        case 3:         //Load/Store
            return arm_load_store(p, val);
        case 4:         //Load/Store
            return arm_load_store_multiple(p, val);
        case 5:         //Branches
            return arm_branch(p, val);
        case 6:         //Coprocessor Load/Store
            return arm_coprocessor_load_store(p, val);
        case 7:
            return arm_coprocessor_others_swi(p, val);
        default:
            return -1;
    }
    return 0;
}

int arm_step(arm_core p) {
    int result;
    result = arm_execute_instruction(p);
    if (result)
        arm_exception(p, result);
    return result;
}