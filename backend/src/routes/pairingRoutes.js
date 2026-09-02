const express = require("express");

const pairingController = require("../controllers/pairingController");
const { pairingRateLimit } = require("../middlewares/rateLimit");

const router = express.Router();

router.post("/claim", pairingRateLimit, pairingController.claim);
router.post(
  "/device-profile-sync",
  pairingRateLimit,
  pairingController.syncProfile,
);

module.exports = router;
