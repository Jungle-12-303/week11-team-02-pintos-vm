# 팀 커밋 컨벤션

## 커밋 메시지

### 형식

```text
<type>: <한국어 제목>
```

본문이 필요한 경우:

```text
<type>: <한국어 제목>

<한국어 본문>
```

Breaking Change가 있는 경우:

```text
<type>: <한국어 제목>

<한국어 본문>

BREAKING CHANGE: explain the incompatible change in English
```

### 허용 타입

- `feat`: 사용자 관점의 기능 추가
- `fix`: 버그 수정
- `refactor`: 동작 변화 없이 구조 개선
- `docs`: 문서 변경
- `test`: 테스트 추가 또는 수정
- `chore`: 잡무성 변경, 유지보수
- `style`: 동작에 영향 없는 포맷팅
- `perf`: 성능 개선
- `build`: 빌드 설정, 패키지, 컴파일 구성 변경
- `ci`: CI 설정 변경
- `revert`: 이전 커밋 되돌리기

### 스코프 정책

커밋 제목에 스코프를 사용하지 않는다.  
변경된 파일과 한국어 제목으로 영향 범위를 전달한다.

### 제목 규칙

- 한국어로 작성
- 한 줄로 유지
- 마침표로 끝내지 않기
- 행위가 아닌 변경의 결과를 서술
- `수정`, `작업`, `변경`, `업데이트` 같은 모호한 단어 지양
- `자식 노드 비교 순서를 바로잡아`, `타입스크립트 빌드 설정을 추가` 같은 구체적 표현 선호

### 본문 규칙

아래 중 하나라도 해당하면 본문 추가:

- 변경 이유가 명확하지 않을 때
- 영향 범위가 넓을 때
- 마이그레이션 또는 사용 주의사항이 있을 때
- 리뷰어에게 컨텍스트가 필요할 때

한국어로 작성, 간결하고 사실에 기반해 서술.

### 예시

```text
feat: DOM 렌더러 초기 구조를 추가

fix: 자식 노드 재정렬 시 인덱스 계산 오류를 고쳐

docs: README에 타입스크립트 시작 방법을 정리

build: TypeScript 출력 경로와 타입 선언 생성을 설정

refactor: 가상 노드 생성 흐름을 단순화
```

본문 포함 예시:

```text
fix: 자식 노드 재정렬 시 인덱스 계산 오류를 고쳐

키 비교 후 재배치 순서를 다시 계산하도록 바꿔
중첩 목록 갱신에서 잘못된 DOM 이동이 발생하지 않게 한다
```

Breaking Change 예시:

```text
feat: 렌더러 초기화 API를 단순화

기본 사용 흐름을 하나로 맞추기 위해 진입 함수를 통합한다

BREAKING CHANGE: replace createRenderer() with createRoot()
```

## 브랜치 전략

### 우리 팀의 브랜치 구조

```text
main (릴리즈 전용 - 항상 동작하는 코드만)
 │
 │  ← PR 머지 (dev → main, 릴리즈 태그 부여)
 │
dev (통합 브랜치 - 팀의 중간 저장소)
 │
 ├── dev/woonyong     ← 개인 작업 브랜치 (PR → dev)
 ├── dev/jihye        ← 개인 작업 브랜치 (PR → dev)
 ├── dev/minsoo       ← 개인 작업 브랜치 (PR → dev)
 │
 └── hotfix/설명      ← 긴급 수정 (main에서 분기 → main+dev 머지)
```

### 브랜치 규칙

- `main`은 직접 커밋/push 금지. PR을 통해서만 머지.
- `dev`도 직접 push 금지. 개인 브랜치에서 PR을 통해서만 머지.
- 개인 브랜치(`dev/<이름>`)는 반드시 `dev`에서 분기.
- `hotfix` 브랜치는 반드시 `main`에서 분기, 수정 후 `main`과 `dev` 양쪽에 PR.

### 흐름 요약

1. `dev`에서 `dev/<이름>` 브랜치 생성
2. 작업 후 `dev/<이름>` → `dev`로 PR 생성
3. 리뷰 후 머지
4. `dev`에서 기능이 완성되고 동작 확인 후 `dev` → `main`으로 PR 생성 + 릴리즈 태그
5. `main`에서 긴급 버그 발생 시 `hotfix/<설명>` 분기 후 `main` PR + `dev` PR

## Merge 정책

### 핵심 원칙

```text
[X] git merge → git push (직접 머지 금지)
[O] GitHub에서 PR 생성 → 리뷰 → Merge 버튼 클릭
```

이 규칙은 `main`, `dev` 모두에 적용된다.  
직접 `merge` 명령어로 `dev`나 `main`에 push하지 않는다.

### 머지 방식

팀 기본 머지 방식은 GitHub PR의 **Merge Commit** 사용이다.

```bash
# 로컬에서 직접 머지하지 않음.
# GitHub PR의 "Merge pull request" 버튼을 사용.
# Squash merge나 Rebase merge는 사용하지 않음.
```

### Rebase 정책

`rebase`는 팀이 Git에 더 익숙해질 때까지 사용하지 않는다.

```text
[X] git rebase
[O] git merge
[O] GitHub PR
```

## Pull Request

### 핵심 규칙

```text
[O] 모든 머지는 반드시 PR을 통해 진행
[O] PR 없이는 dev에도, main에도 머지할 수 없음
[O] 최소 1명 이상의 리뷰 후 머지 (셀프 머지 금지)
```

### PR 종류와 흐름

| PR 종류 | 방향 | 리뷰어 | 용도 |
| --- | --- | --- | --- |
| 일반 PR | `dev/<이름>` → `dev` | 팀원 1명 이상 | 일상적인 기능/수정 머지 |
| 릴리즈 PR | `dev` → `main` | 팀 전체 합의 | 안정 버전 릴리즈 |
| 핫픽스 PR | `hotfix/<설명>` → `main` | 가능한 빨리 1명 | 긴급 수정 |
| 핫픽스 동기화 | `hotfix/<설명>` → `dev` | 자동 또는 1명 | 핫픽스를 `dev`에 반영 |

### PR 제목 형식

```text
<type>: <한국어 설명>
```

예시:

```text
feat: 알람 클록 sleep/wakeup 메커니즘 구현
fix: 타이머 인터럽트에서 tick 비교 오류를 수정
docs: README에 빌드 방법을 추가
```

### 일반 PR 템플릿

```markdown
## 무엇을 변경했나요?

변경 내용을 간단히 설명.

## 왜 변경했나요?

변경 동기와 맥락 설명.

## 주요 변경 파일

- `파일1.c` - 변경 요약
- `파일2.h` - 변경 요약

## 테스트

- [ ] 빌드 성공 (`make` 통과)
- [ ] 관련 테스트 통과
- [ ] 수동 동작 확인

## 셀프 체크리스트

- [ ] 커밋 메시지가 컨벤션을 따르는가
- [ ] 코딩 스타일 가이드를 준수했는가
- [ ] 불필요한 디버그 출력(`printf` 등)을 제거했는가
- [ ] 충돌 없이 `dev`에 머지 가능한가
```

### 릴리즈 PR 템플릿

```markdown
## 릴리즈 버전

v0.1.0

## 포함된 변경사항

- feat: 알람 클록 구현 (#3)
- fix: 타이머 오버플로우 수정 (#5)
- docs: README 업데이트 (#7)

## 테스트 결과

- [ ] 전체 빌드 성공
- [ ] 전체 테스트 통과
- [ ] 팀 전체 동의
```

## 브랜치 명명 규칙

```text
dev/woonyong
dev/jihye
dev/minsoo

hotfix/timer-overflow
hotfix/login-crash
hotfix/null-pointer-in-scheduler
```

사용하지 않는 브랜치 형식:

```text
[X] feature/...
[X] fix/...
[X] release/...
```

## 자주 쓰는 Git 명령어

```bash
# 개인 브랜치 생성 (dev에서 분기)
git checkout dev
git pull origin dev
git checkout -b dev/woonyong

# dev 최신화 반영
git checkout dev && git pull origin dev
git checkout dev/woonyong && git merge dev

# 작업 임시 저장
git stash push -m "WIP: 타이머 작업 중"
git stash pop

# 실수 되돌리기
git reset --soft HEAD~1
git revert HEAD

# 머지 취소 (충돌 중)
git merge --abort
```

## 안티패턴

```text
[X] main이나 dev에 직접 push
[X] PR 없이 로컬에서 git merge로 dev/main에 머지
[X] .env, API 키 커밋
[X] 1000줄 이상의 거대한 PR
[X] "update", "fix", "수정" 같은 모호한 커밋 메시지
[X] 공개 브랜치(dev, main)에 force push
[X] 다른 사람의 dev/<이름> 브랜치에 push
[X] rebase 사용
[X] dist/, node_modules/, build/ 커밋
```
