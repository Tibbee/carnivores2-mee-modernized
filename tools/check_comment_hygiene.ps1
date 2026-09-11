# tools/check_comment_hygiene.ps1
#
# Guards the comments against reference rot. A lot of this project's behaviour
# is explained only in comments, so a comment that points at a document or a
# path nobody can resolve is worse than having no comment there at all.
#
# Errors (exit 1):
#   - a Windows drive path in a comment, i.e. a developer machine leaked in
#   - a section symbol left over from the old design-doc numbering; those
#     documents were archived and renumbered, so the symbol resolves to
#     nothing. Name the topic instead.
#   - a *.md reference with no matching file in the doc tree
#
# Reported but not an error:
#   - "Phase N.N" tags. They track the planning docs, which get reorganised
#     every few months, so they are counted rather than policed.

param(
    [string]$RepoRoot = (Split-Path $PSScriptRoot -Parent),
    [string]$DocRoot = $env:CARNIVORES_DOC_DIR,
    [string[]]$SkipDir = @('deps', 'build', 'install'),
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'

$codeExtensions = @('.c', '.cpp', '.h', '.hpp', '.rc', '.frag', '.vert', '.ps1', '.cmake', '.txt')
$hashCommentExt = @('.ps1', '.cmake', '.txt')
$sectionSymbol  = [char]0x00A7
$drivePathRe    = [regex]'(?<![A-Za-z])[A-Za-z]:[\\/]'
$mdRefRe        = [regex]'(?<![\w.\\-])([\w.\\/-]*\.md)\b'
$phaseRe        = [regex]'\bPhase\s*[0-9][0-9A-Za-z.]*'

# Comment text only. String literals are blanked first so a // or # inside one
# cannot be mistaken for a comment, and a path inside one is not a finding.
function Get-CommentText {
    param([string]$Line, [string]$Extension)

    $s = $Line -replace '"(?:[^"\\]|\\.)*"', '""'
    $s = $s -replace "'(?:[^'\\]|\\.)*'", "''"

    if ($hashCommentExt -contains $Extension) {
        $i = $s.IndexOf('#')
    }
    elseif ($s.TrimStart().StartsWith('*') -or $s.TrimStart().StartsWith('/*')) {
        return $s
    }
    else {
        $i = $s.IndexOf('//')
    }

    if ($i -lt 0) { return '' }
    return $s.Substring($i)
}

$docNames = @{}
if ($DocRoot -and (Test-Path $DocRoot)) {
    Get-ChildItem -Path $DocRoot -Recurse -File -Filter '*.md' -ErrorAction SilentlyContinue |
        ForEach-Object { $docNames[$_.Name] = $_.FullName }
}

$tracked = & git -C $RepoRoot ls-files
if ($LASTEXITCODE -ne 0) { throw "git ls-files failed in $RepoRoot" }

$errors = 0
$phaseTags = 0
$scanned = 0

foreach ($rel in $tracked) {
    $ext = [System.IO.Path]::GetExtension($rel).ToLowerInvariant()
    if ($codeExtensions -notcontains $ext) { continue }

    $skip = $false
    foreach ($dir in $SkipDir) {
        if ($rel -like "$dir/*") { $skip = $true; break }
    }
    if ($skip) { continue }

    $scanned++
    $path = Join-Path $RepoRoot $rel
    $lineNo = 0

    foreach ($line in [System.IO.File]::ReadAllLines($path)) {
        $lineNo++
        $comment = Get-CommentText -Line $line -Extension $ext
        if (-not $comment) { continue }

        if ($comment.Contains($sectionSymbol)) {
            Write-Host ("{0}:{1}  section symbol: {2}" -f $rel, $lineNo, $comment.Trim()) -ForegroundColor Red
            $errors++
        }

        $m = $drivePathRe.Match($comment)
        if ($m.Success) {
            Write-Host ("{0}:{1}  absolute path: {2}" -f $rel, $lineNo, $comment.Trim()) -ForegroundColor Red
            $errors++
        }

        $phaseTags += $phaseRe.Matches($comment).Count

        # URLs are stripped first: a .md inside one is not a local reference.
        $search = $comment -replace 'https?://\S+', ''
        foreach ($ref in $mdRefRe.Matches($search)) {
            $base = Split-Path -Leaf ($ref.Groups[1].Value -replace '\\', '/')
            if ($docNames.Count -gt 0 -and -not $docNames.ContainsKey($base)) {
                Write-Host ("{0}:{1}  missing doc: {2}" -f $rel, $lineNo, $base) -ForegroundColor Red
                $errors++
            }
        }
    }
}

if (-not $Quiet) {
    Write-Host ''
    Write-Host ("scanned {0} files" -f $scanned)
    if ($docNames.Count -gt 0) {
        Write-Host ("doc tree: {0} files" -f $docNames.Count)
    }
    else {
        Write-Host 'doc tree not set (pass -DocRoot or CARNIVORES_DOC_DIR); .md references not checked' -ForegroundColor Yellow
    }
    Write-Host ("Phase tags still present: {0}" -f $phaseTags)
}

if ($errors -gt 0) {
    Write-Host ("comment hygiene: {0} problem(s)" -f $errors) -ForegroundColor Red
    exit 1
}

Write-Host 'comment hygiene: clean' -ForegroundColor Green
exit 0
