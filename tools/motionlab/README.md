# MotionLab offline — fundação ML-0/ML-1

Implementa, **somente offline e com fixtures sintéticas**, a base da [especificação MotionLab](../../docs/research/motionlab-live.md) (ADR-024):

- `motionlab/recorder.py`: sessão `created → recording ⇄ paused → finalizing → finalized | rejected`, ou `interrupted`; chunks CSV com escrita atômica; duplicatas, frames fora de ordem, inválidos e lacunas são **contados e recusados**, nunca escondidos; `manifest.json` com SHA-256 de cada arquivo e taxa efetiva.
- `motionlab/bundle.py`: reimportação que recusa hash divergente, arquivo ausente ou chunk fora do manifest; sessão sem manifest é sempre `interrupted`; anotação de GT separada da intenção e de predições (`source: model` não é aceito; `unknown`/`ambiguous` são válidos); pareamento 1:1 de transições com tolerância pré-declarada.
- Contratos: [`docs/contracts/motionlab`](../../docs/contracts/motionlab).

Não existe stream ao vivo, tópico MQTT de laboratório, tabela MySQL, UI Android, classificador ou sessão com pessoas. Nada aqui é chamado pelo firmware, pela outbox ou pelo SOS.

```bash
python -m unittest discover -s tools/motionlab/tests -v
```
