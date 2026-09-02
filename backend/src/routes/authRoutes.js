const express = require("express");

const authController = require("../controllers/authController");
const { authRateLimit } = require("../middlewares/rateLimit");

const router = express.Router();

router.post("/register", authRateLimit, authController.register);
router.post("/login", authRateLimit, authController.login);

module.exports = router;
