const express = require("express");

const authController = require("../controllers/authController");
const { optionalSession } = require("../middlewares/auth");
const { authRateLimit, refreshRateLimit } = require("../middlewares/rateLimit");

const router = express.Router();

router.post("/register", authRateLimit, authController.register);
router.post("/login", authRateLimit, authController.login);
router.post("/mobile/login", authRateLimit, authController.mobileLogin);
router.post("/mobile/refresh", refreshRateLimit, authController.mobileRefresh);
router.post("/mobile/logout", refreshRateLimit, optionalSession, authController.mobileLogout);

module.exports = router;
