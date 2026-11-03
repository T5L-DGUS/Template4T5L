static void test_handshake(void)
{
    const uint8_t startup[] = {0xAA,0x55,0,5,0x35,0x0A,0,1,1};
    /* Exact V851 example supplied for startup: tag 0x04 is "1.0.0". */
    const uint8_t peer_startup[] = {
        0xAA,0x55,0x00,0x25,0x35,0x09,0x00,0x21,
        0x01,0x00,0x05,0x32,0x2E,0x30,0x2E,0x30,
        0x02,0x00,0x07,0x48,0x57,0x2D,0x56,0x31,0x2E,0x30,
        0x03,0x00,0x04,0x00,0x00,0x00,0x00,
        0x04,0x00,0x05,0x31,0x2E,0x30,0x2E,0x30
    };
    const uint8_t storage[] = {0xAA,0x55,0,11,0x35,0x0A,0,7,1,0,4,0,0,0,1};
    const uint8_t bad_length[] = {0xAA,0x55,0,5,0x35,0x0A,0,0};
    const uint8_t bad_tail[] = {0xAA,0x55,0,11,0x35,0x09,0,4,4,0,1,'X',9,0,1};
    const uint8_t wrong_command[] = {0xAA,0x55,0,4,0x36,0x0A,0,0};
    const uint8_t scan[] = {0xAA,0x55,0,3,0xC0,0,5};
    V851TlvSegment filler = {0x09, NULL, 0};
    unsigned i;

    memset(vp, 0, sizeof(vp));
    memset(&Uart4, 0, sizeof(Uart4));
    hold_uart_busy = 0; mock_tick = 0; sent_count = 0;
    V851ProtocolInit();
    assert(!sent_count && !V851ProtocolPeerBootReceived());
    /* A busy transmitter delays the sole startup notice without dropping it. */
    Uart4.TxBusy = 1;
    V851ProtocolTask(); assert(!sent_count);
    Uart4.TxBusy = 0; Uart4.TxHead = 1;
    V851ProtocolTask(); assert(!sent_count);
    Uart4.TxTail = 1;
    /* Both existing queues may be full; startup still has its own pending bit. */
    assert(V851ProtocolSendSegments(0x35, &filler, 1));
    assert(V851ProtocolSendSegments(0x35, &filler, 1));
    assert(V851ProtocolSendWifiFrame(scan, sizeof(scan)));
    V851ProtocolTask();
    assert(sent_count == 1 && sent_len[0] == sizeof(startup));
    assert(memcmp(sent[0], startup, sizeof(startup)) == 0);
    for(i = 0; i < 10; ++i) V851ProtocolTask();
    assert(sent_count == 4); /* One startup, one Wi-Fi, two ordinary TLV frames. */

    for(i = 0; i < sizeof(peer_startup); ++i)
        V851ProtocolReceiveFrame(peer_startup, (uint16_t)i);
    V851ProtocolReceiveFrame(startup, sizeof(startup));
    V851ProtocolReceiveFrame(bad_length, sizeof(bad_length));
    V851ProtocolReceiveFrame(bad_tail, sizeof(bad_tail));
    V851ProtocolReceiveFrame(wrong_command, sizeof(wrong_command));
    V851ProtocolReceiveFrame(storage, sizeof(storage));
    assert(!V851ProtocolPeerBootReceived());
    assert(vp[0x5290 * 2] == 0 && vp[0x52A0 * 2] == 0);
    assert(!v851_factory_report_pending && !v851_state_dirty_mask);

    /* No response, heartbeat, retry or resync, even without any peer notice. */
    for(i = 1; i <= 180; ++i)
    {
        mock_tick = i * 1000U;
        V851ProtocolTask();
    }
    assert(sent_count == 4);
    V851ProtocolReceiveFrame(peer_startup, sizeof(peer_startup));
    assert(V851ProtocolPeerBootReceived());
    assert(strcmp((char *)(vp + 0x5290 * 2), "1.0.0") == 0);
    assert(strcmp((char *)(vp + 0x52A0 * 2), "1.0.0") == 0);
    for(i = 0; i < 10; ++i)
    {
        V851ProtocolReceiveFrame(peer_startup, sizeof(peer_startup));
        V851ProtocolTask();
    }
    assert(sent_count == 4);
    assert(!v851_factory_report_pending && !v851_state_dirty_mask);

    /* A local reboot sends exactly one new notice and clears the RX latch. */
    InitAfterStartupHandshake();
    assert(!V851ProtocolPeerBootReceived());
    assert(memcmp(sent[0], startup, sizeof(startup)) == 0);
    V851ProtocolTask(); assert(sent_count == 0);
    /* Peer-first startup must not suppress our own one-shot transmission. */
    V851ProtocolInit(); sent_count = 0;
    V851ProtocolReceiveFrame(peer_startup, sizeof(peer_startup));
    V851ProtocolTask();
    assert(sent_count == 1 && V851ProtocolPeerBootReceived());
    assert(memcmp(sent[0], startup, sizeof(startup)) == 0);
    V851ProtocolTask(); assert(sent_count == 1);
}
