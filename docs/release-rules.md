# Release Rules

## Quando atualizar o CHANGELOG.md
Atualizar sempre que houver:
- mudança funcional
- mudança estrutural relevante
- baseline nova
- mudança de setup/execução
- mudança de segurança
- correcao importante
- nova limitação conhecida
- mudança relevante em documentação operacional

## Estrutura recomendada do changelog
Cada versão deve conter:
- `Adicionado`
- `Alterado`
- `Corrigido`
- `Pendente / Faltando`
- `Limitações conhecidas`
- `Divida técnica / Pontos fracos`
- `Proximos passos sugeridos`

## Versionamento
- manter incremento coerente
- sugerir tag quando houver baseline ou release significativa
- não inventar versão sem justificar no changelog

## Política SemVer do projeto integrado (vigente para próximos marcos)

- Uma versão **única de projeto** (hoje `0.10.0-dev.0`) alinha os manifests raiz/backend/frontend e os cabeçalhos das lockfiles, porque são distribuídos e apresentados como uma plataforma integrada. Esses números não afirmam que cada componente foi modificado nesta rodada. O app Android usa a mesma versão como `versionName` e deriva dela um `versionCode` inteiro crescente. No futuro, versões próprias de firmware, aplicativo Android, protocolo e modelo podem ser registradas em *build metadata* separado, mas **não substituir a versão do produto** sem ADR.
- No SemVer, `0.y.z` indica desenvolvimento antes do compromisso estável 1.0; incremento **patch** para correções compatíveis, **minor** para capacidade compatível (numeração 0.x ainda sujeita a contratos), **major** para ruptura. Prerelease `-dev.N`, `-rc.N` é diferente de uma tag de release e deve ser explicitamente rotulada como não publicada.
- Manter `[Unreleased]` para mudanças integradas desde a última versão/tag. A primeira versão futura só é fechada quando CI exato, descrição verificável dos gates, artefatos, documentação, migrations, segurança e limitações forem examinados. **Não marcar HIL nem Android como validados só para fechar uma versão.**
- Não criar retroativamente tags 0.9.5/0.9.6 para PRs anteriores. Tag é imutável; GitHub Release é ação distinta. Regra de arquitetura e gates em [version-milestones.md](roadmap/version-milestones.md).
- CI executa `node scripts/check-version-consistency.mjs` para garantir manifests, lockfiles, formato de versão e link do changelog. Isso **não substitui** CI funcional e HIL.
