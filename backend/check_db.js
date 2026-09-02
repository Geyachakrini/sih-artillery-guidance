const sqlite3 = require("sqlite3").verbose();
const db = new sqlite3.Database("./database.sqlite");

db.all("SELECT * FROM targets", [], (err, rows) => {
  if (err) {
    console.error(err);
  } else {
    console.log("Stored Targets in Database:", rows);
  }
});
db.close();
