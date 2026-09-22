# Diagramas UML

Estas fontes PlantUML representam a arquitetura implementada no projeto.
As visões 02–08 detalham o modelo 3D; a visão 09 mostra a composição 2D,
selecionada na compilação e integrada aos mesmos módulos.

| Arquivo | Tipo | Visão |
| --- | --- | --- |
| [01-module-dependencies.puml](01-module-dependencies.puml) | Pacotes | Dependências permitidas entre módulos |
| [02-composition-root.puml](02-composition-root.puml) | Classes/composição | Objetos criados e conectados pelo executável |
| [03-application-domain-classes.puml](03-application-domain-classes.puml) | Classes | Estado, domínio, portas e controlador |
| [04-command-model.puml](04-command-model.puml) | Classes | Comandos enviados pela UI à aplicação |
| [05-frame-loop-sequence.puml](05-frame-loop-sequence.puml) | Sequência | Processamento completo de um frame |
| [06-load-project-sequence.puml](06-load-project-sequence.puml) | Sequência | Carregamento atômico de um projeto |
| [07-simulation-lifecycle.puml](07-simulation-lifecycle.puml) | Estados | Ciclo de vida de uma sessão de simulação |
| [08-rendering-classes.puml](08-rendering-classes.puml) | Classes | Colaboração entre renderer, simulation e graphics |
| [09-shallow-water.puml](09-shallow-water.puml) | Classes/composição | Modelo 2D nos módulos existentes, com ponto de entrada único |

Para renderizar todos os arquivos com o CLI do PlantUML:

```sh
plantuml docs/uml/*.puml
```

A configuração `.clang-uml` também contém os namespaces atuais para permitir
a geração complementar de diagramas diretamente do código.
