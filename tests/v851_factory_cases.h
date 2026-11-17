static void factory_prepare(uint8_t fill)
{
    memset(factory_nor, fill, sizeof(factory_nor));
    factory_writes = factory_reads = factory_fail_read = 0;
    factory_fail_write = factory_drop_write = factory_damage_write = 0;
    memset(vp, 0, sizeof(vp));
}

static void factory_fix_crc(uint8_t *record)
{
    uint16_t crc = crc_16(record, 254);
    record[254] = (uint8_t)crc;
    record[255] = (uint8_t)(crc >> 8);
}

static void factory_assert_report(const char **values)
{
    static const uint8_t tags[] = {1, 3, 4, 5, 6};
    unsigned i, offset = 8, length;
    unsigned reads_before = factory_reads, writes_before = factory_writes;
    V851ProtocolRequestFactoryReport();
    V851ProtocolTask();
    assert(sent_count == 1 && sent[0][4] == 0x36 && sent[0][5] == 0x6A);
    for(i = 0; i < 5; ++i)
    {
        length = (unsigned)strlen(values[i]);
        assert(sent[0][offset] == tags[i]);
        assert(sent[0][offset + 1] == 0 && sent[0][offset + 2] == length);
        assert(memcmp(sent[0] + offset + 3, values[i], length) == 0);
        offset += 3 + length;
    }
    assert(sent_len[0] == offset && sent[0][3] == offset - 4);
    assert(factory_reads == reads_before && factory_writes == writes_before);
    sent_count = 0;
}

static void test_factory_flash(void)
{
    const char *defaults[] = {"CN", "MCQX_QXY03", "QXY03", "HW-V1.0", "FW-V1.0.0"};
    const char *custom[] = {"JP", "FACTORY_PRODUCT", "CUSTOM_MODEL", "HW-V3.0", "FW-V0.9.0"};
    static const unsigned offsets[] = {8, 16, 80, 144, 176};
    static const unsigned capacities[] = {8, 64, 64, 32, 32};
    uint8_t saved[256], damaged[256], *record = factory_nor + 0x0200;
    char long_values[5][64];
    const char *long_fields[5];
    unsigned i, fill;

    for(fill = 0; fill < 2; ++fill)
    {
        factory_prepare(fill ? 0xFF : 0);
        InitAfterStartupHandshake();
        assert(V851FactoryReady() && factory_writes == 1 && factory_reads == 2);
        assert(memcmp(record, "VFC1\x00\x01\x01\x00", 8) == 0);
        assert(crc_16(record, 256) == 0);
        for(i = 0; i < sizeof(factory_nor); ++i)
            if(i < 0x0200 || i >= 0x0300) assert(factory_nor[i] == (fill ? 0xFF : 0));
        factory_assert_report(defaults);
        memcpy(saved, record, sizeof(saved));
        InitAfterStartupHandshake();
        assert(factory_writes == 1 && memcmp(record, saved, 256) == 0);
        factory_assert_report(defaults);
    }

    /* Factory-programmed values win over the compiled defaults, on every boot. */
    for(i = 0; i < 5; ++i)
    {
        memset(record + offsets[i], 0, capacities[i]);
        memcpy(record + offsets[i], custom[i], strlen(custom[i]));
    }
    factory_fix_crc(record);
    memcpy(saved, record, 256);
    InitAfterStartupHandshake();
    assert(factory_writes == 1);
    factory_assert_report(custom);
    test_key_to_wire_before_flash();
    assert(factory_writes == 1 && memcmp(record, saved, 256) == 0);
    InitAfterStartupHandshake();
    factory_assert_report(custom);

    /* Exercise the full 210-byte payload, beyond the former 80-byte buffer. */
    for(i = 0; i < 5; ++i)
    {
        memset(long_values[i], 'A' + i, capacities[i] - 1);
        long_values[i][capacities[i] - 1] = 0;
        long_fields[i] = long_values[i];
        memcpy(record + offsets[i], long_fields[i], capacities[i]);
    }
    factory_fix_crc(record);
    InitAfterStartupHandshake();
    factory_assert_report(long_fields);

    /* CRC, schema, empty and unterminated strings: preserve bytes, send nothing. */
    for(i = 0; i < 4; ++i)
    {
        memcpy(record, saved, 256);
        if(i == 0) record[16] ^= 1;
        if(i == 1) { record[5] = 2; factory_fix_crc(record); }
        if(i == 2) { record[8] = 0; factory_fix_crc(record); }
        if(i == 3) { memset(record + 16, 'X', 64); factory_fix_crc(record); }
        memcpy(damaged, record, 256);
        InitAfterStartupHandshake();
        assert(!V851FactoryReady() && factory_writes == 1);
        assert(memcmp(record, damaged, 256) == 0);
        V851ProtocolRequestFactoryReport(); V851ProtocolTask();
        assert(!sent_count && !v851_factory_report_pending);
    }

    /* Read/write failures and partial writes must not publish volatile defaults. */
    for(i = 0; i < 5; ++i)
    {
        factory_prepare(0xFF);
        if(i == 0) factory_fail_read = 1;
        if(i == 1) factory_fail_write = 1;
        if(i == 2) factory_drop_write = 1;
        if(i == 3) factory_damage_write = 1;
        if(i == 4) factory_fail_read = 2;
        InitAfterStartupHandshake();
        assert(!V851FactoryReady() && factory_writes == (i == 0 ? 0 : 1));
        V851ProtocolRequestFactoryReport(); V851ProtocolTask();
        assert(!sent_count);
        if(i == 3)
        {
            memcpy(saved, record, 256);
            InitAfterStartupHandshake();
            assert(!V851FactoryReady() && factory_writes == 1);
            assert(memcmp(record, saved, 256) == 0);
        }
    }
    puts("Factory defaults, provisioning, reboot/reset persistence, bounds and I/O failures passed");
}
