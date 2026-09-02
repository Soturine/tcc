function removeDatabaseStatements(sql) {
  return String(sql)
    .replace(
      /^[ \t]*CREATE\s+DATABASE\s+IF\s+NOT\s+EXISTS[\s\S]*?;[ \t]*(?:\r?\n|$)/im,
      "",
    )
    .replace(/^[ \t]*USE[ \t]+[^;\r\n]+;[ \t]*(?:\r?\n|$)/gim, "")
    .trim();
}

module.exports = {
  removeDatabaseStatements,
};
