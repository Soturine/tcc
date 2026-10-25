"""MotionLab offline (ADR-024): fundacao ML-0/ML-1 da especificacao docs/research/motionlab-live.md.

Somente biblioteca padrao. Isolado da protecao critica: nada aqui e chamado pelo
firmware, pela outbox ou pelo caminho de SOS. Dados de sessoes reais ficam fora do Git.
"""

FRAME_SCHEMA_VERSION = 1
MANIFEST_SCHEMA_VERSION = 1
ANNOTATION_SCHEMA_VERSION = 1

# Vocabulario do estudo piloto; "unknown" e "ambiguous" sao respostas validas.
LABELS = (
    "sitting",
    "sit_to_stand",
    "standing",
    "stand_to_sit",
    "other_movement",
    "unknown",
    "ambiguous",
)
TRANSITION_LABELS = ("sit_to_stand", "stand_to_sit")
# Fontes aceitas como ground truth. Predicao de modelo NUNCA e fonte de rotulo.
GT_SOURCES = ("observer", "video_review", "self_report")
