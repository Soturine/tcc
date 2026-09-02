const assert = require("node:assert/strict");
const test = require("node:test");

const { removeDatabaseStatements } = require("../../scripts/sqlUtils");

test("remove USE sem colar o proximo comando a comentario CRLF", () => {
  const sql = [
    "-- senha: demo",
    "",
    "USE queda_monitor;",
    "",
    "INSERT INTO users (name) VALUES ('Demo');",
    "",
  ].join("\r\n");

  const result = removeDatabaseStatements(sql);

  assert.doesNotMatch(result, /^USE\b/m);
  assert.match(result, /-- senha: demo(?:\r\n)+INSERT INTO users/);
});

test("remove CREATE DATABASE multilinha e USE do schema", () => {
  const sql = [
    "CREATE DATABASE IF NOT EXISTS queda_monitor",
    "  CHARACTER SET utf8mb4",
    "  COLLATE utf8mb4_unicode_ci;",
    "",
    "USE queda_monitor;",
    "",
    "CREATE TABLE example (id INT);",
    "",
  ].join("\n");

  const result = removeDatabaseStatements(sql);

  assert.equal(result, "CREATE TABLE example (id INT);");
});
