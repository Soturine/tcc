const mysql = require("mysql2/promise");

const { env } = require("../config/env");

const pool = mysql.createPool({
  host: env.mysql.host,
  port: env.mysql.port,
  user: env.mysql.user,
  password: env.mysql.password,
  database: env.mysql.database,
  waitForConnections: true,
  connectionLimit: 10,
  queueLimit: 0,
  timezone: "Z",
  decimalNumbers: true,
});

async function execute(executor, sql, params = []) {
  const runner = executor || pool;
  const [rows] = await runner.execute(sql, params);
  return rows;
}

async function one(executor, sql, params = []) {
  const rows = await execute(executor, sql, params);
  return rows[0] || null;
}

async function transaction(work, options = {}) {
  const maxAttempts = Math.min(
    Math.max(Number.parseInt(options.maxAttempts, 10) || 3, 1),
    5,
  );

  for (let attempt = 1; attempt <= maxAttempts; attempt += 1) {
    const connection = await pool.getConnection();

    try {
      await connection.beginTransaction();
      const result = await work(connection);
      await connection.commit();
      return result;
    } catch (error) {
      await connection.rollback();

      if (error.code !== "ER_LOCK_DEADLOCK" || attempt === maxAttempts) {
        throw error;
      }
    } finally {
      connection.release();
    }
  }

  throw new Error("Transacao esgotou as tentativas sem resultado.");
}

async function testConnection() {
  const connection = await pool.getConnection();

  try {
    await connection.ping();
  } finally {
    connection.release();
  }
}

module.exports = {
  pool,
  execute,
  one,
  transaction,
  testConnection,
};
