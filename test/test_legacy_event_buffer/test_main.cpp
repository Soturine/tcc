// Caracterizacao do EventBuffer herdado ANTES da outbox critica (auditoria F-05/F-06).
// Estes testes registram o comportamento existente, inclusive os defeitos; nao o endossam.

#include <unity.h>

#include "app_config.h"
#include "event_buffer.h"

namespace {

String payloadFor(int index) {
  return String("{\"event_uuid\":\"evt-") + std::to_string(index) + "\"}";
}

void testFifoOrderIsPreserved() {
  EventBuffer buffer;
  buffer.push("t/events", payloadFor(1), 10);
  buffer.push("t/events", payloadFor(2), 20);

  BufferedEvent event;
  TEST_ASSERT_TRUE(buffer.peek(event));
  TEST_ASSERT_EQUAL_STRING(payloadFor(1).c_str(), event.payload.c_str());
  TEST_ASSERT_TRUE(buffer.pop());
  TEST_ASSERT_TRUE(buffer.peek(event));
  TEST_ASSERT_EQUAL_STRING(payloadFor(2).c_str(), event.payload.c_str());
}

void testOverflowSilentlyDropsOldestCriticalEvent() {
  EventBuffer buffer;
  const size_t capacity = AppConfig::EVENT_BUFFER_CAPACITY;
  for (size_t index = 1; index <= capacity + 1; ++index) {
    // push nunca sinaliza perda: retorna true mesmo descartando o mais antigo.
    TEST_ASSERT_TRUE(buffer.push("t/events", payloadFor(static_cast<int>(index)), index));
  }

  TEST_ASSERT_EQUAL_UINT32(capacity, buffer.size());
  BufferedEvent oldest;
  TEST_ASSERT_TRUE(buffer.peek(oldest));
  TEST_ASSERT_EQUAL_STRING(payloadFor(2).c_str(), oldest.payload.c_str());
}

void testPopRemovesWithoutAnyAcknowledgement() {
  // O chamador (flushBufferedEvents) faz pop logo apos publish() local; nao ha ACK.
  EventBuffer buffer;
  buffer.push("t/events", payloadFor(1), 10);
  TEST_ASSERT_TRUE(buffer.pop());
  TEST_ASSERT_TRUE(buffer.isEmpty());
  TEST_ASSERT_FALSE(buffer.pop());
}

void testRestoreKeepsOnlyWhatSnapshotProvided() {
  // main.cpp persiste no maximo PERSISTED_EVENT_BUFFER_CAPACITY itens; o restante
  // existia apenas em RAM e some no reboot.
  TEST_ASSERT_TRUE(AppConfig::PERSISTED_EVENT_BUFFER_CAPACITY < AppConfig::EVENT_BUFFER_CAPACITY);

  EventBuffer buffer;
  for (int index = 1; index <= 6; ++index) {
    buffer.push("t/events", payloadFor(index), index);
  }
  BufferedEvent snapshot[AppConfig::EVENT_BUFFER_CAPACITY];
  const size_t copied = buffer.copyTo(snapshot, AppConfig::PERSISTED_EVENT_BUFFER_CAPACITY);
  TEST_ASSERT_EQUAL_UINT32(AppConfig::PERSISTED_EVENT_BUFFER_CAPACITY, copied);

  EventBuffer afterReboot;
  afterReboot.restoreFrom(snapshot, copied);
  TEST_ASSERT_EQUAL_UINT32(AppConfig::PERSISTED_EVENT_BUFFER_CAPACITY, afterReboot.size());
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testFifoOrderIsPreserved);
  RUN_TEST(testOverflowSilentlyDropsOldestCriticalEvent);
  RUN_TEST(testPopRemovesWithoutAnyAcknowledgement);
  RUN_TEST(testRestoreKeepsOnlyWhatSnapshotProvided);
  return UNITY_END();
}
