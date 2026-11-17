/* Emulate NOR independently of the DGUS VP space and across protocol restarts. */
static uint8_t factory_nor[0x0800U * 2U];
static unsigned factory_writes, factory_reads, factory_fail_read;
static uint8_t factory_fail_write, factory_drop_write, factory_damage_write;

void T5lNorFlashRW(uint8_t operation, uint8_t block, uint16_t addr,
                   uint16_t cache, uint8_t *buffer, uint16_t words)
{
    assert(block == 0 && addr == 0x0100U && words == 128U);
    assert(addr + words <= 0x0800U && cache == 0x2000U);
    vp[sysDGUS_FLASH_RW_CMD * 2] = 0;
    if(operation == flashREAD_FLAG)
    {
        ++factory_reads;
        if(factory_reads == factory_fail_read)
        {
            vp[sysDGUS_FLASH_RW_CMD * 2] = flashREAD_FLAG;
            return;
        }
        memcpy(buffer, factory_nor + addr * 2, words * 2);
    }
    else
    {
        assert(operation == flashWRITE_FLAG);
        ++factory_writes;
        if(factory_fail_write)
        {
            vp[sysDGUS_FLASH_RW_CMD * 2] = flashWRITE_FLAG;
            return;
        }
        if(!factory_drop_write)
        {
            memcpy(factory_nor + addr * 2, buffer, words * 2);
            if(factory_damage_write) factory_nor[addr * 2 + 20] ^= 1;
        }
    }
}
