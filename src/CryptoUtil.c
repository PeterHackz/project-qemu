#include <CryptoUtil.h>
#include <Library.h>
#include <stdio.h>
#include <string.h>

void CryptoUtil_FindPublicKey(const Library *libg)
{
    int64_t (*load64)(int64_t) = NULL;
    const uint8_t *buf = (uint8_t *)libg->base;
    for (int i = 0; i + 3 < libg->size; i += 4)
    {
        for (int j = 0x20; j < 0x2F; j++)
        {
            /*
             look for blake2b_init_param xor constants
             there should be a `movk [reg], #0x6a09, lsl #48`
             we try all registers.
             */
            const int movk_instr = 0xF2ED4100 | j;
            int value;
            memcpy(&value, buf + i, sizeof(int));
            if (value == movk_instr)
            {
                const int reg = j - 0x20;
                if (libg->size > 16)
                {
                    // our target constant is 0x6A09E667F3BCC908LL
                    // now we check for a `mov [reg], #0xc908`
                    // load64 is before the mov 0x6a09. but we want to make sure
                    // we are looking at the right layout
                    const int instr_post_bl = 0xD2992100 | reg;
                    memcpy(&value, buf + i - 3 * 4, sizeof(int));
                    if (value == instr_post_bl)
                    {
                        int maybe_bl;
                        memcpy(&maybe_bl, buf + i - 4 * 4, sizeof(int));
                        if ((maybe_bl & 0xFC000000) == 0x94000000)
                        {
                            int32_t imm26 = maybe_bl & 0x03FFFFFF;
                            if (imm26 & (1 << 25))
                                imm26 |= ~0x03FFFFFF;
                            const uint64_t instr_addr =
                                libg->base + i -
                                5 * 4 /* 4 movs and instruction itself */;
                            const uint64_t pc = instr_addr + 4;
                            const uint64_t target = pc + ((int64_t)imm26 << 2);
                            load64 = (void *)target;
                            printf("[*] CryptoUtil: found load64 at %p\n",
                                   (void *)(target - libg->base));
                        }
                    }
                }
            }
        }
    }

    if (load64 == NULL)
    {
        printf("[*] CryptoUtil: failed to find load64!\n");
        return;
    }

    uint8_t key[32] = {0};

    for (int64_t i = 0; i < 4; i++)
    {
        int64_t key_chars = load64(i * 8);
        memcpy(key + i * 8, &key_chars, 8);
    }

    printf("[*] CryptoUtil: Server Public Key: ");
    for (int i = 0; i < 32; i++)
    {
        printf("%02x", key[i]);
    }
    printf("\n");
}
