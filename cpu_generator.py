from __future__ import print_function
from collections import OrderedDict
from functools import partial


def parse_template(template_fname):
    with open('templates/' + template_fname, 'rb') as f:
        items = f.read().replace('\n', '').replace(' ', '').split(',')
        assert len(items) == 256
        return items


def get_instruction_list():
    return zip(
        parse_template('opcodes.txt'), parse_template('operand_0.txt'),
        parse_template('operand_1.txt'))


def operand_len(operand):
    keys = operand_decode_table.keys()
    try:
        index = keys.index(operand)
    except ValueError:
        return 0
    if index >= keys.index('kd16'):
        return 2
    if index >= keys.index('kd8'):
        return 1
    return 0


def instruction_len(instruction):
    # TODO: handle CB and oddities like STOP
    return 1 + operand_len(instruction[1]) + operand_len(instruction[2])


def decode_operand(enum):
    if not enum in operand_decode_table:
        raise ValueError('Undecodeable operand: ' + enum)
    return operand_decode_table[enum]


def decode_instruction(instruction):
    assert len(instruction) == 3
    print(instruction)
    op = instruction[0]
    if not op in opcode_table:
        raise ValueError('Unhandleable opcode: ' + op)
    opcode_table[op](instruction[1], instruction[2])
    print('  %s += %d;' % (reg('pc'), instruction_len(instruction)))
    print('  break;')


# Specefic instructions
def handle_ld(dst, src):
    if is_deref(dst):
        if is_16b_reg(src):
            print('  ' + ww(decode_operand(dst), decode_operand(src)) + ';')
        else:
            print('  ' + wb(decode_operand(dst), decode_operand(src)) + ';')
    elif is_deref(src):
        print('  %s = %s;' % (decode_operand(dst), rb(decode_operand(src))))
    else:
        print('  %s = %s;' % (decode_operand(dst), decode_operand(src)))


def handle_inc(dst, _):
    decoded_dst = decode_operand(dst)
    if is_deref(dst):
        print('  ' + wb(decoded_dst, rb(decoded_dst) + 1))
        z(rb(decoded_dst)), freset('n'), h(rb(decoded_dst))
    else:
        print('  ++%s;' % decoded_dst)
        if len(dst) != 3:
            z(decoded_dst), freset('n'), h(decoded_dst)


def handle_dec(dst, _):
    decoded_dst = decode_operand(dst)
    if is_deref(dst):
        print('  ' + wb(decoded_dst, rb(decoded_dst) - 1))
        z(rb(decoded_dst)), fset('n'), h(rb(decoded_dst))
    else:
        print('  --%s;' % decoded_dst)
        if not is_16b_reg(dst):
            z(decoded_dst), fset('n'), h(decoded_dst)


def handle_rl(through_carry, dst, _):
    if is_deref(dst):
        raise ValueError('Unhandled RLC (HL)')
    decoded_dst = decode_operand(dst)
    print('  const uint8_t old_7_bit = (%s & 0x80) >> 7;' % reg(decoded_dst))
    print('  %s <<= 1;' % reg(decoded_dst))
    if through_carry:
        print('  %s |= %s;' % (reg(decoded_dst), reg('f.c')))
    else:
        print('  %s |= old_7_bit;' % reg(decoded_dst))
    # Errata in http://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html
    # RLCA/RLA don't reset z when a is dst
    z(decoded_dst), freset('n'), freset('h'), c('old_7_bit')


def handle_rr(through_carry, dst, _):
    if is_deref(dst):
        raise ValueError('Unhandled RRC (HL)')
    decoded_dst = decode_operand(dst)
    print('  const uint8_t old_0_bit = %s & 0x01;' % reg(decoded_dst))
    print('  %s >>= 1;' % reg(decoded_dst))
    if through_carry:
        print('  %s |= (%s << 7);' % (reg(decoded_dst), reg('f.c')))
    else:
        print('  %s |= (old_0_bit << 7);' % reg(decoded_dst))
    # Errata in http://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html
    # RRCA/RRA don't reset z when a is dst
    z(decoded_dst), freset('n'), freset('h'), c('old_7_bit')


def handle_add(dst, src):
    decoded_dst = decode_operand(dst)
    print('  %s += %s;' % (decoded_dst, decode_operand(src)))
    freset('n'), h(decoded_dst), c(decoded_dst)
    if dst == 'kSP':
        freset('z')
    else:
        z(decoded_dst)


# Utils
def reg(reg):
    if reg == '':
        raise ValueError('invalid reg')
    return 'lr35902->registers.' + reg


def rb(addr):
    return 'rb(lr35902->mmu, %s)' % addr


def wb(addr, val):
    return 'wb(lr35902->mmu, %s, %s)' % (addr, val)


def ww(addr, val):
    return 'ww(lr35902->mmu, %s, %s)' % (addr, val)


def fset(flag):
    print('  %s = 1;' % reg('f.' + flag))


def freset(flag):
    print('  %s = 0;' % reg('f.' + flag))


def z(dst):
    print('  %s = !%s;' % (reg('f.z'), dst))


def h(dst):
    print('  %s = (%s & 0x10) == 0x10;' % (reg('f.h'), dst))


def c(dst):
    print('  %s = %s;' % (reg('f.c'), dst))


def is_deref(operand):
    return operand.startswith('kDEREF')


# ex. kSP
def is_16b_reg(operand):
    return len(operand) == 3


# Main
operand_decode_table = OrderedDict([
    ('kA', reg('a')),
    ('kB', reg('b')),
    ('kC', reg('c')),
    # ('kDEREF_C', ''),
    ('kD', reg('d')),
    # ('kE', ''),
    # ('kH', ''),
    # ('kL', ''),
    # ('kAF', ''),
    ('kBC', reg('bc')),
    ('kDEREF_BC', reg('bc')),
    ('kDE', reg('de')),
    ('kDEREF_DE', reg('de')),
    ('kHL', reg('hl')),
    # ('kDEREF_HL', ''),
    # ('kDEREF_HL_INC', ''),
    # ('kDEREF_HL_DEC', ''),
    ('kSP', reg('sp')),
    # ('kCOND_Z', ''),
    # ('kCOND_NZ', ''),
    # ('kCOND_C', ''),
    # ('kCOND_NC', ''),
    # ('kBIT_0', ''),
    # ('kBIT_1', ''),
    # ('kBIT_2', ''),
    # ('kBIT_3', ''),
    # ('kBIT_4', ''),
    # ('kBIT_5', ''),
    # ('kBIT_6', ''),
    # ('kBIT_7', ''),
    # ('kLITERAL_0x00', ''),
    # ('kLITERAL_0x08', ''),
    # ('kLITERAL_0x10', ''),
    # ('kLITERAL_0x18', ''),
    # ('kLITERAL_0x20', ''),
    # ('kLITERAL_0x28', ''),
    # ('kLITERAL_0x30', ''),
    # ('kLITERAL_0x38', ''),
    ('kd8', 'load_d8(lr35902)'),
    # ('kr8', ''),
    # ('kSP_r8', ''),
    # ('kDEREF_a8', ''),
    ('kd16', 'load_d16(lr35902)'),
    # ('ka16', ''),
    ('kDEREF_a16', 'load_d16(lr35902)'),
])

opcode_table = {
    'kNOP': lambda x, y: None,
    'kLD': handle_ld,
    'kINC': handle_inc,
    'kDEC': handle_dec,
    'kRLC': partial(handle_rl, False),
    'kADD': handle_add,
    'kRRC': partial(handle_rr, False),
    'kSTOP': lambda x, y: print('  %s += 1;' % reg('pc')),
    'kRL': partial(handle_rl, True),
}

for i, instruction in enumerate(get_instruction_list()):
    print('case 0x%s:' % format(i, '02X'))
    decode_instruction(instruction)
    if i == 23:
        break
