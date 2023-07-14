# Template design documentation <your design title here>

## Background
Give a brief overview of your proposed design and the motivation for it

## Object diagram
Explain how your design works at the block level
```plantuml
@startuml
package ProposedComponent {
object "subComponent1" as c1
object "subComponent2" as c2
c2 <|- c1

@enduml
```

## Relationships
Explain how your design interfaces with other existing components
```plantuml
@startuml
package LCEVC_DEC {
object ProposedComponent
object coreDecoder
ProposedComponent -> coreDecoder : fixes

@enduml
```

## Usage
If required, explain how any APIs or features will be used
```plantuml
@startuml
actor client
client -> ProposedComponent : API

@enduml
```
If applicable, include API documentation for any new or modified endpoints

## Other
Any other details that reviewers should be aware of
