const assert = require("node:assert/strict");
const test = require("node:test");

const mysqlModulePath = require.resolve("mysql2/promise");
const poolModulePath = require.resolve("../../src/db/pool");

function loadPoolWithConnections(connections) {
  const previousMysql = require.cache[mysqlModulePath];
  const previousPool = require.cache[poolModulePath];
  const fakePool = {
    getConnection: async () => connections.shift(),
  };

  delete require.cache[poolModulePath];
  require.cache[mysqlModulePath] = {
    id: mysqlModulePath,
    filename: mysqlModulePath,
    loaded: true,
    exports: {
      createPool: () => fakePool,
    },
  };

  const loaded = require(poolModulePath);

  return {
    module: loaded,
    restore() {
      delete require.cache[poolModulePath];

      if (previousMysql) {
        require.cache[mysqlModulePath] = previousMysql;
      } else {
        delete require.cache[mysqlModulePath];
      }

      if (previousPool) {
        require.cache[poolModulePath] = previousPool;
      }
    },
  };
}

function fakeConnection() {
  return {
    beginTransactionCalls: 0,
    commitCalls: 0,
    rollbackCalls: 0,
    releaseCalls: 0,
    async beginTransaction() {
      this.beginTransactionCalls += 1;
    },
    async commit() {
      this.commitCalls += 1;
    },
    async rollback() {
      this.rollbackCalls += 1;
    },
    release() {
      this.releaseCalls += 1;
    },
  };
}

test("transaction repete somente deadlock e usa nova conexao", async () => {
  const first = fakeConnection();
  const second = fakeConnection();
  const { module: database, restore } = loadPoolWithConnections([first, second]);
  let workCalls = 0;

  try {
    const result = await database.transaction(async () => {
      workCalls += 1;

      if (workCalls === 1) {
        const error = new Error("deadlock");
        error.code = "ER_LOCK_DEADLOCK";
        throw error;
      }

      return "ok";
    });

    assert.equal(result, "ok");
    assert.equal(workCalls, 2);
    assert.equal(first.rollbackCalls, 1);
    assert.equal(first.releaseCalls, 1);
    assert.equal(second.commitCalls, 1);
    assert.equal(second.releaseCalls, 1);
  } finally {
    restore();
  }
});

test("transaction nao repete erro que nao seja deadlock", async () => {
  const connection = fakeConnection();
  const { module: database, restore } = loadPoolWithConnections([connection]);
  let workCalls = 0;

  try {
    await assert.rejects(
      database.transaction(async () => {
        workCalls += 1;
        const error = new Error("invalid input");
        error.code = "ER_BAD_NULL_ERROR";
        throw error;
      }),
      (error) => error.code === "ER_BAD_NULL_ERROR",
    );

    assert.equal(workCalls, 1);
    assert.equal(connection.rollbackCalls, 1);
    assert.equal(connection.releaseCalls, 1);
  } finally {
    restore();
  }
});
